// In speedtest.c

#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "../include/network.h"
#include "../include/speedtest.h"

double test_latency(const char* host) {
    struct timeval start, end;
    int sockfd = create_tcp_socket();
    
    gettimeofday(&start, NULL);
    
    // Attempt connection (simplified HTTP GET)
    if (connect_to_server(sockfd, host, 80) < 0) {
        return -1.0;
    }
    
    gettimeofday(&end, NULL);
    close(sockfd);
    
    // Calculate RTT in milliseconds
    double latency_ms = (end.tv_sec - start.tv_sec) * 1000.0;
    latency_ms += (end.tv_usec - start.tv_usec) / 1000.0;
    
    return latency_ms;
}

double test_download_speed(const char* host, int port, int duration_sec) {
    int sockfd = create_tcp_socket();
    connect_to_server(sockfd, host, port);
    
    char buffer[8192];
    size_t total_bytes = 0;
    time_t start_time = time(NULL);
    time_t current_time;
    
    while ((current_time = time(NULL)) - start_time < duration_sec) {
        ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) break;
        total_bytes += bytes_received;
    }
    
    double elapsed = difftime(current_time, start_time);
    double speed_mbps = (total_bytes * 8.0) / (elapsed * 1000000.0);
    
    close(sockfd);
    return speed_mbps;
}

double test_upload_speed(const char* host, int port, int duration_sec) {
    int sockfd = create_tcp_socket();
    connect_to_server(sockfd, host, port);
    
    char data[8192] = {0}; // Zero-filled buffer
    size_t total_bytes = 0;
    time_t start_time = time(NULL);
    time_t current_time;
    
    while ((current_time = time(NULL)) - start_time < duration_sec) {
        ssize_t bytes_sent = send(sockfd, data, sizeof(data), 0);
        if (bytes_sent <= 0) break;
        total_bytes += bytes_sent;
    }
    
    double elapsed = difftime(current_time, start_time);
    double speed_mbps = (total_bytes * 8.0) / (elapsed * 1000000.0);
    
    close(sockfd);
    return speed_mbps;
}
