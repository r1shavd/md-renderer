#ifndef SERVER_MONGOOSE_H
#define SERVER_MONGOOSE_H

void signal_handler(int signal);    // for handling SIGKILL && SIGTERM
void start_httpserver(const char* host, int port);

#endif    /* SERVER_MONGOOSE_H */
