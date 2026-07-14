#ifndef MDRENDERER_HELPERS_H

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 8080

#ifdef _WIN32
    #include <windows.h>
    #include <dirent.h>
    
    #define make_directory(path) _mkdir(path)
    #define unlink _unlink
    #define rmdir _rmdir
#else
    #include <dirent.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
    
    #define make_directory(path) mkdir(path, 0777)
#endif    // WINDOWS - LINUX make_directory() DEFINITION

#endif    // MDRENDERER_HELPERS_H

extern char* DEFAULT_PATH;

int delete_directory(const char* path);
void setup(int *repeat);
void cleanup();
int validate_httpport(const char* port);
