#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>


int create_tcp_socket() {
    return socket(AF_INET, SOCK_STREAM, 0);
}

int connect_to_server(int sockfd, const char* host, int port) {
    struct hostent *server = gethostbyname(host);
    if (server == NULL) return -1;
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    return connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
}
