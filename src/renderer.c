#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "renderer.h"

int renderMarkdownFile(const char* markdownfile, char* htmlfile) {
	/*
	This function renders the markdown file data from markdown file
	and then creates a temporary html file to host at the server.

	Requires arguments:
	markdownfile -> string (char*)
	htmlfile -> string (char*)

	Returns:
	EXIT_FAILURE -> failed to render
	EXIT_SUCCESS -> successfully rendered
	*/

	// Reading the markdown file
    FILE *markdownfile_ptr = fopen(markdownfile, "rb");
    if (markdownfile_ptr == NULL) {
        // If the markdown file failed to open

		if (errno == ENOENT)	printf("[%s!%s] File not found: %s%s%s\n", RED, DEFAULT, RED, markdownfile, DEFAULT);
		else if (errno == EACCES)	printf("[%s!%s] Permission denied : %s%s%s\n", RED, DEFAULT, RED, markdownfile, DEFAULT);
		else	printf("[%s!%s] Failed accessing: %s%s%s\n   %s", RED, DEFAULT, RED, markdownfile, DEFAULT, strerror(errno));

		return EXIT_FAILURE;
    }

    // Reading the file size
    // - - -
    if (fseek(markdownfile_ptr, 0, SEEK_END) != 0) {
    	// If it fails to seek end of the file 

	    printf("[!] Error seeking to end of file: %s\n", strerror(errno));
	    fclose(markdownfile_ptr);
	    return EXIT_FAILURE;
	}
	//
	long file_size = ftell(markdownfile_ptr);
	if (file_size == -1L) {
		// If it fails counting the file size

	    printf("[!] Error measuring file size: %s\n", strerror(errno));
	    fclose(markdownfile_ptr);
	    return EXIT_FAILURE;
	}
	//
	if (fseek(markdownfile_ptr, 0, SEEK_SET) != 0) {
		// If it fails to rewing the file seek

	    printf("[!] Error rewinding file: %s\n", strerror(errno));
	    fclose(markdownfile_ptr);
	    return EXIT_FAILURE;
	}
	// - - -

    // Dynamically allocating a string to capture all the data
    char *markdownData = malloc( file_size+1 );
    if (!markdownData) {
        fprintf(stderr, "Memory allocation failed! File is too large.\n");
        fclose(markdownfile_ptr);
        return EXIT_FAILURE;
    }

    // Reading the markdown file data
    size_t bytes_read = fread(markdownData, 1, file_size, markdownfile_ptr);
    if (bytes_read != (size_t) file_size && ferror(markdownfile_ptr)) {
	    // If the fread() fails, we exit after cleanup

	    printf("[%s!%s] File reading error. %s%s%s\n", RED, DEFAULT, WHITE, strerror(errno), DEFAULT);
	    free(markdownData);
	    fclose(markdownfile_ptr);
	    return EXIT_FAILURE;
	}
    markdownData[bytes_read] = '\0'; // Null-terminate it like a C string
    fclose(markdownfile_ptr);
    printf("[%s~%s] %sContents reading done: %s%s%s\n", GREEN, WHITE, DEFAULT, YELLOW, markdownfile, DEFAULT);

    // Creating a new html file to store the rendered data temporarily
    FILE *htmlfile_ptr = fopen(htmlfile, "wb");
    if (!htmlfile_ptr) {
        // If there is an error with HTML file creation, we exit after cleanup

        if (errno == EACCES)	printf("[%s!%s] %sPermission denied at configured directory%s\n", RED, DEFAULT, WHITE, DEFAULT);
       	else	printf("[%s!%s] %s%s%s\n", RED, DEFAULT, WHITE, strerror(errno), DEFAULT);

       	free(markdownData);
        return EXIT_FAILURE;
    }

    // Rendering the markdown file data to HTML version
    char *htmlData = cmark_markdown_to_html(markdownData, bytes_read, CMARK_OPT_DEFAULT);
    free(markdownData);
    printf("[%s~%s] Markdown file data rendering done\n", YELLOW, DEFAULT);

    if (!htmlData) {
    	// If the conversion of markdown data via cmark lib route fails

    	printf("[%s!%s] %sFailed to render the markdown data%s\n", RED, DEFAULT, WHITE, DEFAULT);
    	fclose(htmlfile_ptr);
    	return EXIT_FAILURE;
    }

    // Saving the size of the converted data[bytes_read] size
    // There will be a significant difference in size due to rendering
    // like * gets converted to <li> .. </li> by the parser
    size_t bytes_html = strlen(htmlData);

    // Writing the rendered data to html file
    size_t bytes_written = fwrite(htmlData, 1, bytes_html, htmlfile_ptr);
    if (bytes_written < bytes_html) {
    	// If the file writting fails (like in terms of size converted and size written)
    	// We exit after cleaning up

    	fprintf(stderr, "[%s!%s] %s\n", RED, DEFAULT, strerror(errno));
    	free(htmlData);
    	fclose(htmlfile_ptr);
    	return EXIT_FAILURE;
    }

    // Exiting and cleaning in case of task done
    free(htmlData);

	if (fclose(htmlfile_ptr) == EOF) {
	    printf("[%s!%s] %s%s%s\n",
	           RED, DEFAULT, WHITE, strerror(errno), DEFAULT);
	    return EXIT_FAILURE;
	}

    printf("[%s~%s] Markdown data rendered to %s%s%s\n", YELLOW, DEFAULT, WHITE, htmlfile, DEFAULT);
	return EXIT_SUCCESS;
}