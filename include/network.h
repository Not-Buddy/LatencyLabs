#ifndef NETWORK_H
#define NETWORK_H

int create_tcp_socket();
int connect_to_server(int sockfd, const char* host, int port);

#endif
