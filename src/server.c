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
static volatile sig_atomic_t RUNNING = 1;
//
void SignalHandler(int signal) {
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
static const char* resolveMimeType(const char *fileExtension) {
	/*
	 Maps a file extension string to the correct HTTP Content-Type value.
	 Returns "application/octet-stream" for any unknown extension so the
	 browser always receives a valid MIME type.

	 Requires argument:
	 fileExtension -> const string (without leading dot, e.g. "html", "png")
	*/

	if (strcasecmp(fileExtension, "html") == 0 ||
	    strcasecmp(fileExtension, "htm")  == 0) return "text/html";
	if (strcasecmp(fileExtension, "css")  == 0) return "text/css";
	if (strcasecmp(fileExtension, "js")   == 0) return "application/javascript";
	if (strcasecmp(fileExtension, "txt")  == 0) return "text/plain";
	if (strcasecmp(fileExtension, "png")  == 0) return "image/png";
	if (strcasecmp(fileExtension, "jpg")  == 0 ||
	    strcasecmp(fileExtension, "jpeg") == 0) return "image/jpeg";
	if (strcasecmp(fileExtension, "gif")  == 0) return "image/gif";
	if (strcasecmp(fileExtension, "svg")  == 0) return "image/svg+xml";
	if (strcasecmp(fileExtension, "ico")  == 0) return "image/x-icon";
	if (strcasecmp(fileExtension, "webp") == 0) return "image/webp";
	if (strcasecmp(fileExtension, "json") == 0) return "application/json";

	return "application/octet-stream";
}
//
static int isSafePath(const char *path) {
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

void buildHttpResponse(const char *filename, const char *fileExtension, char *response, size_t *responseLength) {
	/*
	 Reads a file from disk and writes a complete HTTP/1.1 response
	 (headers + body) into the caller-supplied response buffer.

	 On any error the buffer is filled with an appropriate HTTP error
	 response so the caller can always call send() on it safely.

	 Requires arguments:
	 filename        -> string  (path on disk, e.g. "public/index.html")
	 fileExtension   -> string  (without dot, e.g. "html")
	 response        -> char[]  (output buffer, must be >= BUFFER_SIZE * 2)
	 responseLength  -> size_t* (receives the number of bytes written)
	*/

	// Initializig to ensure caller never reads garbage on early return
	*responseLength = 0;

	const char *mimeType = resolveMimeType(fileExtension);

	// Opening the requested file safely (read-only)
	int file_fd = open(filename, O_RDONLY);
	if (file_fd == -1) {
		// Fallback 404 response when the file cannot be opened
		*responseLength = (size_t) snprintf(response, BUFFER_SIZE,
		    "HTTP/1.1 404 Not Found\r\n"
		    "Content-Type: text/plain\r\n"
		    "Connection: close\r\n"
		    "\r\n"
		    "404 - Not Found: %s", filename);
		return;
	}

	// Retrieving file metadata to learn the exact byte count
	struct stat file_stat;
	if (fstat(file_fd, &file_stat) == -1) {
		// fstat failure — send a 500 rather than leaving responseLength at 0
		close(file_fd);
		*responseLength = (size_t) snprintf(response, BUFFER_SIZE,
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
	int headerLength = snprintf(header, sizeof(header),
	    "HTTP/1.1 200 OK\r\n"
	    "Content-Type: %s\r\n"
	    "Content-Length: %ld\r\n"
	    "Connection: close\r\n"
	    "\r\n",
	    mimeType, (long) file_size);

	// Copying the header into the start of our response buffer
	memcpy(response, header, (size_t) headerLength);
	*responseLength = (size_t) headerLength;

	// Reading the entire file body in one call.
	// Capping the read at (BUFFER_SIZE*2 - headerLength - 1) bytes to prevent response boundaries overflow
	size_t maxAllowedFileBytes = (BUFFER_SIZE * 2) - (size_t) headerLength - 1;
	size_t bytesToRead = ((size_t) file_size > maxAllowedFileBytes)
	                         ? maxAllowedFileBytes
	                         : (size_t) file_size;

	ssize_t bytesRead = read(file_fd, response + *responseLength, bytesToRead);
	if (bytesRead > 0) {
		*responseLength += (size_t) bytesRead;
	}

	close(file_fd);
}

void* handleClient(void *arg) {
	/*
	 Thread entry point. Reads one HTTP request from the connected client,
	 validates it, resolves the requested file inside public/, and sends
	 the HTTP response.

	 Only GET requests are accepted. Non-GET methods receive 405.
	 Path traversal attempts receive 403.
	 Missing files receive 404 (via buildHttpResponse).

	 Requires argument:
	 arg -> int* (heap-allocated client socket file descriptor; freed here)
	*/

	// Getting the client side file descriptor
	int clientFileDesc = *(int *) arg;
	free(arg);    // Cleaning up to avoid any memory-block overlap

	char *buffer = (char *) malloc(BUFFER_SIZE * sizeof(char));
	if (!buffer) {
		// If the malloc for the request buffer fails
		
		printf("[!] Http server failed to allocate memory for request buffer. %s\n", strerror(errno));
		close(clientFileDesc);
		return NULL;
	}

	// Receiving the request from the client
	ssize_t bytesReceived = recv(clientFileDesc, buffer, BUFFER_SIZE - 1, 0);
	if (bytesReceived <= 0) {
		// If no request received, or failed to receive
		
		free(buffer);
		close(clientFileDesc);
		return NULL;
	}
	buffer[bytesReceived] = '\0';

	// Checking that the request starts with "GET"
	// Else, we reject with 405 Method (Not Allowed)
	if (strncmp(buffer, "GET ", 4) != 0) {
		const char *methodNotAllowed =
		    "HTTP/1.1 405 Method Not Allowed\r\n"
		    "Content-Type: text/html\r\n"
		    "Content-Length: 91\r\n"
		    "Allow: GET\r\n"
		    "Connection: close\r\n"
		    "\r\n"
		    "<html><body><h1>405 Method Not Allowed</h1>"
		    "<p>Only GET requests are accepted.</p></body></html>";
		send(clientFileDesc, methodNotAllowed, strlen(methodNotAllowed), 0);
		free(buffer);
		close(clientFileDesc);
		return NULL;
	}

	// Compiling the regex to extract the URL from the GET HTTP req
	//
	// Pattern : GET /<path> HTTP/1.x — captures the path component
	regex_t regex;
	if (regcomp(&regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED) != 0) {
		printf("[!] Http server failed to compile request regex. %s\n",
		       strerror(errno));
		free(buffer);
		close(clientFileDesc);
		return NULL;
	}

	regmatch_t matches[2];
	if (regexec(&regex, buffer, 2, matches, 0) != 0) {
		// Request line did not match expected format — malformed request
		const char *badRequest =
		    "HTTP/1.1 400 Bad Request\r\n"
		    "Content-Type: text/plain\r\n"
		    "Connection: close\r\n"
		    "\r\n"
		    "400 - Bad Request";
		send(clientFileDesc, badRequest, strlen(badRequest), 0);
		regfree(&regex);
		free(buffer);
		close(clientFileDesc);
		return NULL;
	}
	regfree(&regex);

	// Extracting the path segment captured by the regex (group 1)
	int pathStart  = (int) matches[1].rm_so;
	int pathEnd    = (int) matches[1].rm_eo;
	int pathLength = pathEnd - pathStart;

	char requestedPath[512] = {0};
	if (pathLength > 0 && pathLength < (int) sizeof(requestedPath) - 1) {
		memcpy(requestedPath, buffer + pathStart, (size_t) pathLength);
		requestedPath[pathLength] = '\0';
	}
	// GET / with an empty captured path maps to index.html
	if (requestedPath[0] == '\0') {
		strncpy(requestedPath, "index.html", sizeof(requestedPath) - 1);
	}

	free(buffer);

	// Guarding against directory traversal before building the disk path
	char validationPath[520];
	snprintf(validationPath, sizeof(validationPath), "/%s", requestedPath);
	if (!isSafePath(validationPath)) {
		const char *forbidden =
		    "HTTP/1.1 403 Forbidden\r\n"
		    "Content-Type: text/html\r\n"
		    "Content-Length: 78\r\n"
		    "Connection: close\r\n"
		    "\r\n"
		    "<html><body><h1>403 Forbidden</h1>"
		    "<p>Directory traversal denied.</p></body></html>";
		send(clientFileDesc, forbidden, strlen(forbidden), 0);
		close(clientFileDesc);
		return NULL;
	}

	// Building the filesystem path inside public/
	char diskPath[600];
	snprintf(diskPath, sizeof(diskPath), "public/%s", requestedPath);

	// Extracting the file extension from the disk path for MIME resolution
	const char *dot = strrchr(diskPath, '.');
	const char *fileExtension = (dot && dot != diskPath) ? dot + 1 : "";

	// Allocating the response buffer and building + sending the HTTP response
	char *response = (char *) malloc(BUFFER_SIZE * 2 * sizeof(char));
	if (!response) {
		printf("[!] Http server failed to allocate memory for response buffer. %s\n",
		       strerror(errno));
		close(clientFileDesc);
		return NULL;
	}

	size_t responseLength = 0;
	buildHttpResponse(diskPath, fileExtension, response, &responseLength);

	if (send(clientFileDesc, response, responseLength, 0) == -1) {
		printf("[!] Failed to send response for %s. %s\n",
		       diskPath, strerror(errno));
	}

	free(response);
	close(clientFileDesc);
	return NULL;
}

int startLocalServer(const char *host, int port) {
	/*
	 Creates a TCP/HTTP server that serves static files from the public/
	 directory. Runs until SIGINT or SIGTERM is received, then closes cleanly.

	 Requires arguments:
	 host -> const string (IPv4 address string, e.g. "127.0.0.1")
	 port -> int
	*/

	// Creating the server socket file-descriptor
	int serverFileDesc = socket(AF_INET, SOCK_STREAM, 0);
	if (serverFileDesc < 0) {
		printf("[!] Error: server socket creation failed. %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	// Allowing the port to be reused immediately after a restart
	int opt = 1;
	setsockopt(serverFileDesc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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
			printf("[!] Invalid IP address format: %s\n", host);
		} else {
			printf("[!] inet_pton error. %s\n", strerror(errno));
		}
		close(serverFileDesc);
		return EXIT_FAILURE;
	}

	// Binding the socket to the designated address and port
	if (bind(serverFileDesc, (struct sockaddr *) &address, sizeof(address)) < 0) {
		printf("[!] Binding to %s:%d failed. %s\n", host, port, strerror(errno));
		close(serverFileDesc);
		return EXIT_FAILURE;
	}

	// Starting to listen for incoming TCP connections
	if (listen(serverFileDesc, 10) < 0) {
		printf("[!] Http server listen() failed. %s\n", strerror(errno));
		close(serverFileDesc);
		return EXIT_FAILURE;
	}

	// Intializing sigaction to handle the signals RUNNING for clean exit
	struct sigaction sa;
	sa.sa_handler = SignalHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;  // SA_RESTART deliberately NOT set
	sigaction(SIGINT,  &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	printf("Visit: http://%s:%d\n", host, port);

	while (RUNNING) {
		// Building the fd_set with only the server socket in it

		fd_set readFds;
		FD_ZERO(&readFds);
		FD_SET(serverFileDesc, &readFds);

		// Setting RUNNING to be re-evaluated at least once per second (even without signals)
		struct timespec timeout;
		timeout.tv_sec  = 1;
		timeout.tv_nsec = 0;

		// Using pselect() as it automatically waits for server socket to become readable
		// Combining with SIGINT to return -1/EINTR, thus we exit RUNNING cleanly
		int ready = pselect(serverFileDesc + 1, &readFds, NULL, NULL, &timeout, NULL);
		if (ready < 0) {
			if (errno == EINTR) continue;  // Signal fired — loop back and check RUNNINGi

			// Else, we faced error via pselect
			printf("[!] Http server failed - pselect(). %s\n", strerror(errno));
			break;
		}
		if (ready == 0) continue;  // 1-second timeout — loop back and check RUNNING

		// A client is ready — accept() will not block here
		struct sockaddr_in clientAddr;
		socklen_t clientAddrLength = sizeof(clientAddr);

		int *clientFileDesc = malloc(sizeof(int));
		if (!clientFileDesc) continue;

		*clientFileDesc = accept(serverFileDesc,
		                         (struct sockaddr *) &clientAddr,
		                         &clientAddrLength);
		if (*clientFileDesc < 0) {
			free(clientFileDesc);
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
				continue;
			}
			printf("[!] Connection request failed. %s\n", strerror(errno));
			continue;
		}

		// Listening to the client in multiple threads to ensure speed
		pthread_t threadId;
		if (pthread_create(&threadId, NULL, handleClient, (void *) clientFileDesc) != 0) {
			// Thread creation failed — clean up and move on
			printf("[!] Failed to create client thread. %s\n", strerror(errno));
			close(*clientFileDesc);
			free(clientFileDesc);
			continue;
		}
		pthread_detach(threadId);
	}

	close(serverFileDesc);
	return EXIT_SUCCESS;
}
