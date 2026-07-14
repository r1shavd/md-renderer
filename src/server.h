#ifndef SERVER_H
#define SERVER_H

void signal_handler(int signal);    // for handling SIGKILL && SIGTERM
void build_http_response(const char* filename, const char* file_extension, char* response, size_t* response_length);
void* handle_client(void *arg);
int start_httpserver(const char* host, int port);

#endif    /* SERVER_H */
