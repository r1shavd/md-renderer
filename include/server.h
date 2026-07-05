#ifndef SERVER_H
#define SERVER_H

void SignalHandler(int signal);    // for handling SIGKILL && SIGTERM
void buildHttpResponse(const char* filename, const char* fileExtension, char* response, size_t* responseSize);
void* handleClient(void *arg);
int startLocalServer(const char* host, int port);

#endif
