#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "parser.h"

int parse_markdown(const char* markdownfile, const char* htmlfile)
{
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
	    fprintf(stderr, "[!] Error seeking to end of file: %s\n", strerror(errno));
	    fclose(markdownfile_ptr);
	    return EXIT_FAILURE;
	}
	//
	long file_size = ftell(markdownfile_ptr);
	if (file_size == -1L) {
	    fprintf(stderr, "[!] Error measuring file size: %s\n", strerror(errno));
	    fclose(markdownfile_ptr);
	    return EXIT_FAILURE;
	}
	//
	if (fseek(markdownfile_ptr, 0, SEEK_SET) != 0) {
	    fprintf(stderr, "[!] Error rewinding file: %s\n", strerror(errno));
	    fclose(markdownfile_ptr);
	    return EXIT_FAILURE;
	}
	// - - -

    // Dynamically allocating a string to capture all the data
    char *markdown_contents = malloc( file_size+1 );
    if (!markdown_contents) {
        fprintf(stderr, "Memory allocation failed! File is too large.\n");
        fclose(markdownfile_ptr);
        return EXIT_FAILURE;
    }

    // Reading the markdown file data
    size_t bytes_read = fread(markdown_contents, 1, file_size, markdownfile_ptr);
    if (bytes_read != (size_t) file_size && ferror(markdownfile_ptr)) {
	    fprintf(stderr, "[%s!%s] File reading error. %s%s%s\n", RED, DEFAULT, WHITE, strerror(errno), DEFAULT);
	    free(markdown_contents);
	    fclose(markdownfile_ptr);
	    return EXIT_FAILURE;
	}

    markdown_contents[bytes_read] = '\0'; // Null-terminate it like a C string
    fclose(markdownfile_ptr);
    printf("[~] Contents reading done: %s\n", markdownfile);


    FILE *htmlfile_ptr = fopen(htmlfile, "wb");
    if (!htmlfile_ptr) {
        if (errno == EACCES)
            fprintf(stderr, "[!] Permission denied at configured directory\n");
        else
            fprintf(stderr, "[!] %s\n", strerror(errno));

       	free(markdown_contents);
        return EXIT_FAILURE;
    }

    // Rendering the markdown data to HTML 
    char *html_contents = cmark_markdown_to_html(markdown_contents, bytes_read, CMARK_OPT_DEFAULT);
    free(markdown_contents);
    printf("[~] Markdown file data rendering done\n");

    if (!html_contents) {
    	fprintf(stderr, "[%s!%s] %sFailed to render the markdown data%s\n", RED, DEFAULT, WHITE, DEFAULT);
    	fclose(htmlfile_ptr);
    	return EXIT_FAILURE;
    }

    // Saving the size of the converted data[bytes_read] size
    // There will be a significant difference in size due to rendering
    // like * gets converted to <li> .. </li> by the parser
    size_t bytes_html = strlen(html_contents);

    // Writing the rendered data to html file
    size_t bytes_written = fwrite(html_contents, 1, bytes_html, htmlfile_ptr);
    if (bytes_written < bytes_html) {
    	fprintf(stderr, "[!] %s\n", strerror(errno));
    	free(html_contents);
    	fclose(htmlfile_ptr);
    	return EXIT_FAILURE;
    }

    free(html_contents);

	if (fclose(htmlfile_ptr) == EOF) {
	    printf("[%s!%s] %s%s%s\n",
	           RED, DEFAULT, WHITE, strerror(errno), DEFAULT);
	    return EXIT_FAILURE;
	}

    printf("[~] Markdown data rendered to %s\n", htmlfile);
	return EXIT_SUCCESS;
}
