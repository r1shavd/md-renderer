#ifndef SERVER_H
#define SERVER_H

void SignalHandler(int signal);    // for handling SIGKILL && SIGTERM
void startLocalServer(const char* port, char* host);

#endif