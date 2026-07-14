#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include "colors.h"
//#include "cmark/cmark.h"
#include "parser.h"
#include "helpers.h"

#ifdef MONGOOSE    // If the MONGOOSE lib is used for web server instead of native http socks version

#include "mongoose/mongoose.h"
#include "server_mongoose.h"

#else

#include "server.h"

#endif    // MONGOOSE lib is being used

#define FILE_UPDATE_EVENT_BUFF_SIZE (1024 * (sizeof(struct inotify_event) + 16))

struct ParsedFiles {
    const char* markdownfile;
    const char* htmlfile;
};

extern volatile sig_atomic_t RUNNING;

void* file_update_watch(void* arg)
{
    /*
     This function checks if the given markdown file is updated, then it calls to parse again.
     This makes sure the file is being parsed live on server.
    */

    struct ParsedFiles* files = (struct ParsedFiles*) arg;
    const char* markdownfile = files->markdownfile;
    const char* htmlfile = files->htmlfile;
    
    int inotify_fd  = inotify_init1(IN_NONBLOCK);
    if (inotify_fd == -1) {
        fprintf(stderr, "[!] Failed to initialize inotify watch on markdown file\n");
        return NULL;
    }

    int watch_fd = inotify_add_watch(
            inotify_fd,
            markdownfile,
            IN_CLOSE_WRITE | IN_MODIFY | IN_MOVED_TO
    );

    if (watch_fd == -1) {
        fprintf(stderr, "[!] Failed to add inotify watch to markdown file\n");
        return NULL;
    }

    char buffer[FILE_UPDATE_EVENT_BUFF_SIZE];
    while (RUNNING) {
        int length = read(inotify_fd, buffer, sizeof(buffer));
        
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000);
                continue;
            }

            fprintf(stderr, "[!] Failed to read inotify updates on markdown file\n");
            break;
        }

        if (length > 0) {
            printf("[~] Refresh the web page. Source file updated: %s\n", markdownfile);

            if (parse_markdown(markdownfile, htmlfile) != 0)
                fprintf(stderr, "[!] Failed to parse markdown file: %s", markdownfile);
        }
    }

    inotify_rm_watch(inotify_fd, watch_fd);
    close(inotify_fd);
    return NULL;
}

void print_help_section()
{
    /*
	 This function displays the help text / usage documentation for the program.
	 The function is called when the user launches the tool using arguments:

	 md-renderer [arguments]
	 	--help, -h
	*/

    printf("%sUsage:%s md-renderer  <markdown-file.md> <host-address> <port>\n", GREEN, DEFAULT);
    printf("       md-renderer [options]\n\n");

    printf("%sA lightweight tool to parse Markdown files into HTML and serve them locally.%s\n\n", CYAN, DEFAULT);

    printf("%sArguments:%s\n", YELLOW, DEFAULT);
    printf("  %s<markdown-file.md>%s   Path to the Markdown file you want to render.\n", BLUE, DEFAULT);
    printf("  %s--host <host-address>%s       IP address to bind the server to (e.g., 127.0.0.1).\n", BLUE, DEFAULT);
    printf("  %s--port <port>%s               Port number to host the server on (e.g., 8080).\n\n", BLUE, DEFAULT);

    printf("%sOptions:%s\n", YELLOW, DEFAULT);
    printf("  %s--help%s           Display this help menu and exit.\n", MAGENTA, DEFAULT);
    printf("  %s--version%s        Display version information and exit.\n\n", MAGENTA, DEFAULT);
    // Although i haven't assigned a fking version. consider this 1.0 as of first 100 updates

    printf("%sExample:%s\n", YELLOW, DEFAULT);
    printf("  md-renderer  README.md --host 127.0.0.1 --port 8080\n\n");

    printf("%sNote:%s Open your web browser and navigate to http://<host-address>:<port> after starting.\n", RED, DEFAULT);
    printf("Also, if %s--host%s or %s--port%s is not provided, then default values (%s %d) are used\n", CYAN, DEFAULT, CYAN, DEFAULT, DEFAULT_HOST, DEFAULT_PORT);
}

int main(int argc, const char** argv)
{
    // The main driver function

    int status = 0;
    setup(&status); // __init__
    if (status != 0) {
        return EXIT_FAILURE;
    }

    atexit(cleanup);    // Calling the cleanup function on exit to avoid any mess

    // Accessing and validating the arguments
    // - - -
    const char* markdownfile = NULL;
	char* host = DEFAULT_HOST;
    int port = DEFAULT_PORT;

    if (argc < 2) {
        printf("[%s!%s] No input mardown files mentioned. Use %s--help%s for usage.\n", RED, DEFAULT, WHITE, DEFAULT);
       return EXIT_FAILURE;
    } 

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_help_section();
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--host") == 0) {
           if (i+1 >= argc) {
               // If the next positional argument is not mentioned
               // i.e., value for --ip-address argument is not mentioned 
               
               printf("[%s!%s] No values give for argument '%s%s%s'.\n", RED, DEFAULT, WHITE, argv[i], DEFAULT);
               return EXIT_FAILURE;
           }
           host = (char*) argv[++i];
        } else if (strcmp(argv[i], "--port") == 0) {
           if (i+1 >= argc) {
               // If the next positional argument is not mentioned
               // i.e., value for --port / -p argument is not mentioned 
               
               printf("[%s!%s] No values give for argument '%s%s%s'.\n", RED, DEFAULT, WHITE, argv[i], DEFAULT);
               return EXIT_FAILURE;
           }
           
           port = validate_httpport(argv[++i]);
           if (port == 0)   return EXIT_FAILURE;
        } else {
            // Any other positional arguments go as source markdown file, but only one file first file will be considered

            if (argv[i][0] == '-') {
                fprintf(stderr, "[%s!%s] Unknown option '%s'. Use --help for usage.\n", RED, DEFAULT, argv[i]);
                return EXIT_FAILURE;
            }
            if (markdownfile != NULL) {
                fprintf(stderr, "[%s!%s] Multiple markdown files specified.\n", RED, DEFAULT);
                return EXIT_FAILURE;
            }
            markdownfile = argv[i];
        }
    }
    // - - -

    char htmlfile[25];
    snprintf(htmlfile, sizeof(htmlfile), "%s/index.html", DEFAULT_PATH);
    
    // Declaring ParsedFiles object
    struct ParsedFiles* files =  malloc(sizeof(struct ParsedFiles));
    files->markdownfile = markdownfile;
    files->htmlfile = (const char*) htmlfile;

    if (parse_markdown(markdownfile, htmlfile) != 0) {
        fprintf(stderr, "[!] Failed to parse markdown file: %s", markdownfile);
    }
    
    pthread_t file_update_watch_thread;
    if (pthread_create(
                &file_update_watch_thread,
                NULL,
                file_update_watch,
                files) != 0) {
        fprintf(stderr, "[!] Failed to create live watch thread\n");
        return EXIT_FAILURE;
    }

    start_httpserver(host, port);
    pthread_join(file_update_watch_thread, NULL);

    return 0;
}
