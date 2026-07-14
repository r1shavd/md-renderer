#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 65536

#include "server.h"
#include "colors.h"

// Helper functions
// - - -
volatile sig_atomic_t RUNNING = 1;
//
void signal_handler(int signal)
{
	/*
	 This function helps in catching the exit signals.
	 It catches SIGINT or SIGTERM and sets RUNNING to 0,
	 causing the pselect() loop to break and the server to shut down cleanly.

	 NOTE: We use sigaction() with SA_RESTART explicitly cleared so that
	 SIGINT interrupts any blocking syscall (pselect) and returns EINTR.
	 The plain signal() call sets SA_RESTART on Linux/glibc, which would
	 silently restart accept() and make Ctrl+C appear to do nothing.
	*/

	(void) signal;
	RUNNING = 0;
}
//
static const char* resolve_mime_type(const char *file_extension)
{
	/*
	 Maps a file extension string to the correct HTTP Content-Type value.
	 Returns "application/octet-stream" for any unknown extension so the
	 browser always receives a valid MIME type.

	 Requires argument:
	 file_extension -> const string (without leading dot, e.g. "html", "png")
	*/

	if (strcasecmp(file_extension, "html") == 0 ||
	    strcasecmp(file_extension, "htm")  == 0) return "text/html";
	if (strcasecmp(file_extension, "css")  == 0) return "text/css";
	if (strcasecmp(file_extension, "js")   == 0) return "application/javascript";
	if (strcasecmp(file_extension, "txt")  == 0) return "text/plain";
	if (strcasecmp(file_extension, "png")  == 0) return "image/png";
	if (strcasecmp(file_extension, "jpg")  == 0 ||
	    strcasecmp(file_extension, "jpeg") == 0) return "image/jpeg";
	if (strcasecmp(file_extension, "gif")  == 0) return "image/gif";
	if (strcasecmp(file_extension, "svg")  == 0) return "image/svg+xml";
	if (strcasecmp(file_extension, "ico")  == 0) return "image/x-icon";
	if (strcasecmp(file_extension, "webp") == 0) return "image/webp";
	if (strcasecmp(file_extension, "json") == 0) return "application/json";

	return "application/octet-stream";
}

static int is_safe_path(const char *path)
{
	/*
	 Validates that the requested path does not attempt directory traversal.
	 Rejects any path containing ".." segments so that requests such as
	 GET /../../etc/passwd cannot escape the public/ directory.

	 Requires argument:
	 path -> const string (the URL path component, e.g. "/index.html")

	 Returns 1 if the path is safe, 0 if it should be rejected.
	*/

	if (!path || path[0] == '\0') return 0;

	const char *cursor = path;
	while (*cursor) {
		// Skipping leading slashes between segments
		while (*cursor == '/') cursor++;
		if (cursor[0] == '.' && cursor[1] == '.' &&
		    (cursor[2] == '/' || cursor[2] == '\0')) {
			return 0;  // Directory traversal detected — reject request instantly
		}
		// Advancing past this segment
		while (*cursor && *cursor != '/') cursor++;
	}

	return 1;
}
// - - -

void build_http_response(const char *filename, const char *file_extension, char *response, size_t *response_len)
{
	/*
	 Reads a file from disk and writes a complete HTTP/1.1 response
	 (headers + body) into the caller-supplied response buffer.

	 On any error the buffer is filled with an appropriate HTTP error
	 response so the caller can always call send() on it safely.

	 Requires arguments:
	 filename        -> string  (path on disk, e.g. "public/index.html")
	 file_extension   -> string  (without dot, e.g. "html")
	 response        -> char[]  (output buffer, must be >= BUFFER_SIZE * 2)
	 response_len  -> size_t* (receives the number of bytes written)
	*/

	*response_len = 0;
	const char *mimeType = resolve_mime_type(file_extension);

	int file_fd = open(filename, O_RDONLY);
	if (file_fd == -1) {
		// Fallback 404 response when the file cannot be opened
		*response_len = (size_t) snprintf(response, BUFFER_SIZE,
		    "HTTP/1.1 404 Not Found\r\n"
		    "Content-Type: text/plain\r\n"
		    "Connection: close\r\n"
		    "\r\n"
		    "404 - Not Found: %s", filename);
		return;
	}

	struct stat file_stat;
	if (fstat(file_fd, &file_stat) == -1) {
		close(file_fd);
		*response_len = (size_t) snprintf(response, BUFFER_SIZE,
		    "HTTP/1.1 500 Internal Server Error\r\n"
		    "Content-Type: text/plain\r\n"
		    "Connection: close\r\n"
		    "\r\n"
		    "500 - Internal Server Error");
		return;
	}
	off_t file_size = file_stat.st_size;

	// Formatting proper HTTP header + explicit Content-Length.
	char header[512];
	int header_len = snprintf(header, sizeof(header),
	    "HTTP/1.1 200 OK\r\n"
	    "Content-Type: %s\r\n"
	    "Content-Length: %ld\r\n"
        "Cache-Control: no-store, no-cache, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
	    "Connection: close\r\n"
	    "\r\n",
	    mimeType, (long) file_size);

	// Copying the header into the start of our response buffer
	memcpy(response, header, (size_t) header_len);
	*response_len = (size_t) header_len;

	// Capping the read at (BUFFER_SIZE*2 - header_len - 1) bytes to prevent response boundaries overflow
	size_t max_allowed_fbytes = (BUFFER_SIZE * 2) - (size_t) header_len - 1;
	size_t bytes_to_read = ((size_t) file_size > max_allowed_fbytes)
	                         ? max_allowed_fbytes
	                         : (size_t) file_size;

	ssize_t bytes_done_read = read(file_fd, response + *response_len, bytes_to_read);
	if (bytes_done_read > 0) {
		*response_len += (size_t) bytes_done_read;
	}

	close(file_fd);
}

void* handle_client(void *arg)
{
	/*
	 Thread entry point. Reads one HTTP request from the connected client,
	 validates it, resolves the requested file inside public/, and sends
	 the HTTP response.

	 Only GET requests are accepted. Non-GET methods receive 405.
	 Path traversal attempts receive 403.
	 Missing files receive 404 (via build_http_response).

	 Requires argument:
	 arg -> int* (heap-allocated client socket file descriptor; freed here)
	*/

	int client_fd = *(int *) arg;
	free(arg);

	char *buffer = (char *) malloc(BUFFER_SIZE * sizeof(char));
	if (!buffer) {
		fprintf(stderr, "[!] Http server failed to allocate memory for request buffer. %s\n", strerror(errno));
		close(client_fd);
		return NULL;
	}

    // Receving req data
	ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
	if (bytes_received <= 0) {
		// If no request received, or failed to receive
		
		free(buffer);
		close(client_fd);
		return NULL;
	}
	buffer[bytes_received] = '\0';

	// Checking that the request starts with "GET"
	// Else, we reject with 405 Method (Not Allowed)
	if (strncmp(buffer, "GET ", 4) != 0) {
		const char *method_not_allowed =
		    "HTTP/1.1 405 Method Not Allowed\r\n"
		    "Content-Type: text/html\r\n"
		    "Content-Length: 91\r\n"
		    "Allow: GET\r\n"
		    "Connection: close\r\n"
		    "\r\n"
		    "<html><body><h1>405 Method Not Allowed</h1>"
		    "<p>Only GET requests are accepted.</p></body></html>";
		send(client_fd, method_not_allowed, strlen(method_not_allowed), 0);
		free(buffer);
		close(client_fd);
		return NULL;
	}

	// Compiling the regex to extract the URL from the GET HTTP req
	// Pattern : GET /<path> HTTP/1.x — captures the path component
	regex_t regex;
	if (regcomp(&regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED) != 0) {
		fprintf(stderr, "[!] Http server failed to compile request regex. %s\n",
		       strerror(errno));
		free(buffer);
		close(client_fd);
		return NULL;
	}

	regmatch_t matches[2];
	if (regexec(&regex, buffer, 2, matches, 0) != 0) {
		const char *bad_request =
		    "HTTP/1.1 400 Bad Request\r\n"
		    "Content-Type: text/plain\r\n"
		    "Connection: close\r\n"
		    "\r\n"
		    "400 - Bad Request";
		send(client_fd, bad_request, strlen(bad_request), 0);
		regfree(&regex);
		free(buffer);
		close(client_fd);
		return NULL;
	}
	regfree(&regex);

	// Extracting the path segment captured by the regex (group 1)
	int path_start  = (int) matches[1].rm_so;
	int path_end    = (int) matches[1].rm_eo;
	int path_len = path_end - path_start;

	char requested_path[512] = {0};
	if (path_len > 0 && path_len < (int) sizeof(requested_path) - 1) {
		memcpy(requested_path, buffer + path_start, (size_t) path_len);
		requested_path[path_len] = '\0';
	}
	// GET / with an empty captured path maps to index.html
	if (requested_path[0] == '\0') {
		strncpy(requested_path, "index.html", sizeof(requested_path) - 1);
	}

	free(buffer);

	// Guarding against directory traversal before building the disk path
	char validation_path[520];
	snprintf(validation_path, sizeof(validation_path), "/%s", requested_path);
	if (!is_safe_path(validation_path)) {
		const char *forbidden =
		    "HTTP/1.1 403 Forbidden\r\n"
		    "Content-Type: text/html\r\n"
		    "Content-Length: 78\r\n"
		    "Connection: close\r\n"
		    "\r\n"
		    "<html><body><h1>403 Forbidden</h1>"
		    "<p>Directory traversal denied.</p></body></html>";
		send(client_fd, forbidden, strlen(forbidden), 0);
		close(client_fd);
		return NULL;
	}

	// Building the filesystem path inside public/
	char disk_path[600];
	snprintf(disk_path, sizeof(disk_path), "public/%s", requested_path);

	// Extracting the file extension from the disk path for MIME resolution
	const char *dot = strrchr(disk_path, '.');
	const char *file_extension = (dot && dot != disk_path) ? dot + 1 : "";

	char *response = (char *) malloc(BUFFER_SIZE * 2 * sizeof(char));
	if (!response) {
		fprintf(stderr, "[!] Http server failed to allocate memory for response buffer. %s\n", strerror(errno));
		close(client_fd);
		return NULL;
	}

	size_t response_len = 0;
	build_http_response(disk_path, file_extension, response, &response_len);

	if (send(client_fd, response, response_len, 0) == -1) {
		fprintf(stderr, "[!] Failed to send response for %s. %s\n", disk_path, strerror(errno));
	}

	free(response);
	close(client_fd);
	return NULL;
}

int start_httpserver(const char *host, int port)
{
	/*
	 Creates a TCP/HTTP server that serves static files from the public/
	 directory. Runs until SIGINT or SIGTERM is received, then closes cleanly.

	 Requires arguments:
	 host -> const string (IPv4 address string, e.g. "127.0.0.1")
	 port -> int
	*/

	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		printf("[%s!%s] Error: server socket creation failed. %s\n", RED, DEFAULT, strerror(errno));
		return EXIT_FAILURE;
	}

	// Allowing the port to be reused immediately after a restart
	int opt = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// Setting up the socket addressing structure
	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port   = htons(port);

	// Converting the string IP address to network binary format
	// host <string> -> address.sin_addr <32-bit network order>
	int status = inet_pton(AF_INET, host, &address.sin_addr);
	if (status <= 0) {
		if (status == 0) {
			fprintf(stderr, "[!] Invalid IP address format: %s\n", host);
		} else {
			fprintf(stderr, "[!] inet_pton error. %s\n", strerror(errno));
		}
		close(server_fd);
		return EXIT_FAILURE;
	}

	if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
		fprintf(stderr, "[!] Binding to %s:%d failed. %s\n", host, port, strerror(errno));
		close(server_fd);
		return EXIT_FAILURE;
	}

	if (listen(server_fd, 10) < 0) {
		fprintf(stderr, "[!] Http server listen() failed. %s\n", strerror(errno));
		close(server_fd);
		return EXIT_FAILURE;
	}

	// Intializing sigaction to handle the signals RUNNING for clean exit
	struct sigaction sa;
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;  // SA_RESTART deliberately NOT set
	sigaction(SIGINT,  &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	printf("Visit: http://%s:%d\n", host, port);

	while (RUNNING) {
		fd_set read_fd;
		FD_ZERO(&read_fd);
		FD_SET(server_fd, &read_fd);

		// Setting RUNNING to be re-evaluated at least once per second (even without signals)
		struct timespec timeout;
		timeout.tv_sec  = 1;
		timeout.tv_nsec = 0;

		// Using pselect() as it automatically waits for server socket to become readable
		// Combining with SIGINT to return -1/EINTR, thus we exit RUNNING cleanly
		int ready = pselect(server_fd + 1, &read_fd, NULL, NULL, &timeout, NULL);
		if (ready < 0) {
			if (errno == EINTR) continue;  // Signal fired — loop back and check RUNNINGi

			fprintf(stderr, "[!] Http server failed - pselect(). %s\n", strerror(errno));
			break;
		}
		if (ready == 0) continue;  // 1-second timeout — loop back and check RUNNING

		// A client is ready — accept() will not block here
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		int *client_fd = malloc(sizeof(int));
		if (!client_fd) continue;

		*client_fd = accept(server_fd,
		                         (struct sockaddr *) &client_addr,
		                         &client_addr_len);
		if (*client_fd < 0) {
			free(client_fd);
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
				continue;
			}
			fprintf(stderr, "[!] Connection request failed. %s\n", strerror(errno));
			continue;
		}

		pthread_t thread_id;
		if (pthread_create(&thread_id, NULL, handle_client, (void *) client_fd) != 0) {
			// Thread creation failed — clean up and move on
			fprintf(stderr, "[!] Failed to create client thread. %s\n", strerror(errno));
			close(*client_fd);
			free(client_fd);
			continue;
		}
		pthread_detach(thread_id);
	}

	close(server_fd);
	return EXIT_SUCCESS;
}
