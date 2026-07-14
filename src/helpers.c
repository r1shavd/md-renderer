#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "helpers.h"

// Defining the default path to store the parsed files
char* DEFAULT_PATH = "public";

int delete_directory(const char *path)
{
    /*
     Helper function to delete a directory and entire tree
    */

    DIR *dir = opendir(path);
    size_t path_len = strlen(path);

    if (!dir) {
        return -1;
    }

    struct dirent *tmp;
    while ((tmp = readdir(dir))) {
        if (strcmp(tmp->d_name, ".") == 0 ||
                strcmp(tmp->d_name, "..") == 0) {
            continue;
        }

        size_t len = path_len + strlen(tmp->d_name) + 2; 
        char *buf = malloc(len);
        
        if (buf) {
            struct stat statbuf;
            snprintf(buf, len, "%s/%s", path, tmp->d_name);

            if (!stat(buf, &statbuf)) {
                if (S_ISDIR(statbuf.st_mode)) {
                    delete_directory(buf);
                } else {
                    unlink(buf);
                }
            }
            free(buf);
        }
    }
    closedir(dir);
    return rmdir(path);
}

int validate_httpport(const char* port)
{
    /*
     Validates HTTP PORT input and returns type <int> port value, if valid.
     if not valid, returns 0.
    */

    if (port == NULL || *port == '\0') {
        return 0;
    }

    char* endptr;
    errno = 0; 
    long port_num = strtol(port, &endptr, 10);
    if (errno == ERANGE) {
        return 0;
    }

    // If endptr doesn't point to the null terminator, trailing garbage exists
    if (*endptr != '\0') {
        return 0;
    }

    if (endptr == port) {
        return 0;
    }

    if (port_num > 1 || port_num < 65535) {
        return port_num;
    }

    return 0;
}

void setup(int *status)
{
    /*
     Runs at startup.
     Create DEFAULT_PATH if doesn't exists, if exists then swaps for public_mdrenderer
    */

    if (*status > 1)    return;

    if (make_directory(DEFAULT_PATH) != 0) {
        if (errno == EEXIST && *status == 0) {
            
            DEFAULT_PATH = "public_mdrenderer";
            *status = 1;
            setup(status);
        } else if (errno == ENOENT) {
            fprintf(stderr, "Directory location error\n");
        } else {
            fprintf(stderr, "Error faced while creating directory\n");
        }
        return;
    }
    *status = 0;
    printf("[~] Created directory to store: %s\n", DEFAULT_PATH);
}

void cleanup()
{
    /*
    Currently removes the rendered HTML files saved.
    */

    if (delete_directory(DEFAULT_PATH) != 0) {
        // The rendered html files cleanup failed

        if (errno == ENOENT)     return;    // If the error is due to folder/file not existing
        printf("[!] Failed to clean up the rendered files\n");
    }
}
