#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "colors.h"
#include "cmark/cmark.h"
#include "mongoose/mongoose.h"
#include "server.h"
#include "renderer.h"

void cleanup() {
    /*
    This function cleans up the created files during the process.
    Currently removes the rendered HTML files saved.
    */

    if (remove("public/index.html") != 0) {
        // The rendered html files cleanup failed

        printf("[%s!%s] Failed to clean up the rendered files\n", RED, DEFAULT);
    }
}

int main(int argc, const char** argv) {
	// The main driver function
    
	atexit(cleanup);
    
    if (renderMarkdownFile(argv[1], "public/index.html") == EXIT_SUCCESS) {
        // If the files are successfully rendered to HTML files

        // Launching the server
        startLocalServer("192.168.1.169", 3000);
    }
    
    return 0;
}
