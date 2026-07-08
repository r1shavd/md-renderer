#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "colors.h"
#include "cmark/cmark.h"
#include "mongoose/mongoose.h"
#include "server.h"
#include "renderer.h"

void printHelpSection() {
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
    printf("  %s<host-address>%s       IP address to bind the server to (e.g., 127.0.0.1).\n", BLUE, DEFAULT);
    printf("  %s<port>%s               Port number to host the server on (e.g., 8080).\n\n", BLUE, DEFAULT);

    printf("%sOptions:%s\n", YELLOW, DEFAULT);
    printf("  %s-h, --help%s           Display this help menu and exit.\n", MAGENTA, DEFAULT);
    printf("  %s-v, --version%s        Display version information and exit.\n\n", MAGENTA, DEFAULT);

    printf("%sExample:%s\n", YELLOW, DEFAULT);
    printf("  md-renderer  README.md 127.0.0.1 8080\n\n");

    printf("%sNote:%s Open your web browser and navigate to http://<host-address>:<port> after starting.\n", RED, DEFAULT);
}

void cleanup() {
    /*
    This function cleans up the created files during the process.
    Currently removes the rendered HTML files saved.
    */

    if (remove("public/index.html") != 0) {
        // The rendered html files cleanup failed

        if (errno == ENOENT)     return;    // If the error is due to folder/file not existing
        printf("[%s!%s] Failed to clean up the rendered files\n", RED, DEFAULT);
    }
}

int main(int argc, const char** argv) {
    // The main driver function

    atexit(cleanup);    // Calling the cleanup function on exit to avoid any mess

    // Accessing and validating the arguments
    // - - -
    const char* markdownfile = NULL;
	char* host = "127.0.0.1";
    int port = 8080;

    if (argc < 2) {
        // If the user doesn't mention any input files (markdown), we exit

        printf("[%s!%s] No input mardown files mentioned. Use %s--help%s for usage.\n", RED, DEFAULT, WHITE, DEFAULT);
       return EXIT_FAILURE;
    } 
    for (int i = 1; i < argc; i++) {
        // Looping through the arguments
     
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            // If the user called for help argument

            printHelpSection();
        } else if (strcmp(argv[i], "--ip-address") == 0 || strcmp(argv[i], "-i") == 0) {
            // If the user mentioned a custom host ip address to bind to the http server

           if (i+1 >= argc) {
               // If the next positional argument is not mentioned
               // i.e., value for --ip-address argument is not mentioned 
               
               printf("[%s!%s] No values give for argument '%s%s%s'.\n", RED, DEFAULT, WHITE, argv[i], DEFAULT);
               return EXIT_FAILURE;
           }
           host = (char*) argv[++i];    // If the value to the args is given properly, then we read and assign values
        } else if (strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) {
            // If the user mentioned a cucstom port to bind to the http server

           if (i+1 >= argc) {
               // If the next positional argument is not mentioned
               // i.e., value for --port / -p argument is not mentioned 
               
               printf("[%s!%s] No values give for argument '%s%s%s'.\n", RED, DEFAULT, WHITE, argv[i], DEFAULT);
               return EXIT_FAILURE;
           }

		   // Parsing and validating the user given port input
		   // If actual port between these ranges (1, 65535], then only allow
		   char *endptr;
		   long port_ = strtol(argv[++i], &endptr, 10);
		   if (*endptr != '\0' || port_ < 1 || port_ > 65535) {
               // If the port is invalid
			   
			   printf("[%s!%s] Given port not in range 1-65535\n", RED, DEFAULT);
			   return EXIT_FAILURE;
		   }
           port = (int) port_;    // Re-assigning the port value from <long> to <int>
        } else {
            // Any other positional arguments go as source markdown file, but only one file first file will be considered

            if (markdownfile != NULL) {
                // If markdown file already mentioned in the pisitonal arguments

				if (strncmp(argv[i], "-", 1) == 0)    printf("[%s!%s] Unknown option '%s%s%s'. Use %s--help%s argument for usage.\n", RED, DEFAULT, WHITE, argv[i], DEFAULT, CYAN, DEFAULT);
				else    printf("[%s!%s] Multiple markdown files specified.\n", RED, DEFAULT);
                return EXIT_FAILURE;
            }
            markdownfile = (char*) argv[i];    // Else, we asign the value to the markdownfile mem loc
        }
    }
    // - - -

    if (renderMarkdownFile(markdownfile, "public/index.html") == EXIT_SUCCESS) {
        // If the files are successfully rendered to HTML files

        // Launching the server
        startLocalServer(host, port);
    }
    
    return 0;
}
