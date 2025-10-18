#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include "../include/network.h"
#include "../include/speedtest.h"

double test_latency(const char* host) {
    struct timeval start, end;
    int sockfd = create_tcp_socket();
    
    gettimeofday(&start, NULL);
    
    if (connect_to_server(sockfd, host, 80) < 0) {
        close(sockfd);
        return -1.0;
    }
    
    gettimeofday(&end, NULL);
    close(sockfd);
    
    double latency_ms = (end.tv_sec - start.tv_sec) * 1000.0;
    latency_ms += (end.tv_usec - start.tv_usec) / 1000.0;
    
    return latency_ms;
}

double test_download_speed(const char* host, int port, int duration_sec) {
    int sockfd = create_tcp_socket();
    
    if (connect_to_server(sockfd, host, port) < 0) {
        close(sockfd);
        return -1.0;
    }
    
    // Set socket receive timeout
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Request a large test file
    char request[1024];
    snprintf(request, sizeof(request),
             "GET /100MB.bin HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: LatencyLabs/1.0\r\n"
             "Connection: close\r\n"
             "\r\n", host);
    
    if (send(sockfd, request, strlen(request), 0) < 0) {
        close(sockfd);
        return -1.0;
    }
    
    char buffer[65536];  // 64KB buffer for faster downloads
    size_t total_bytes = 0;
    int in_headers = 1;
    
    // Start timing immediately
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    while (1) {
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;
        
        if (in_headers) {
            // Skip headers - look for \r\n\r\n
            for (int i = 0; i < bytes - 3; i++) {
                if (buffer[i] == '\r' && buffer[i+1] == '\n' && 
                    buffer[i+2] == '\r' && buffer[i+3] == '\n') {
                    in_headers = 0;
                    // Count data after headers
                    total_bytes += (bytes - (i + 4));
                    break;
                }
            }
        } else {
            total_bytes += bytes;
        }
        
        // Check if we've been running long enough
        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) + 
                        (end.tv_usec - start.tv_usec) / 1000000.0;
        
        if (elapsed >= duration_sec) {
            break;
        }
    }
    
    gettimeofday(&end, NULL);
    close(sockfd);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_usec - start.tv_usec) / 1000000.0;
    
    if (elapsed == 0) elapsed = 0.001;
    
    double speed_mbps = (total_bytes * 8.0) / (elapsed * 1000000.0);
    
    return speed_mbps;
}

double test_upload_speed(const char* host, int port, int duration_sec) {

    int sockfd = create_tcp_socket();
    
    if (connect_to_server(sockfd, host, port) < 0) {
        close(sockfd);
        return -1.0;
    }
    
    // Prepare upload data
    size_t chunk_size = 65536;  // 64KB chunks
    char *data = malloc(chunk_size);
    if (!data) {
        close(sockfd);
        return -1.0;
    }
    memset(data, 'A', chunk_size);
    
    // Calculate total size to upload
    size_t total_size = chunk_size * 100;  // ~6.4 MB
    
    // Send POST header
    char header[512];
    snprintf(header, sizeof(header),
             "POST /upload HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/octet-stream\r\n"
             "Content-Length: %zu\r\n"
             "\r\n", host, total_size);
    
    if (send(sockfd, header, strlen(header), 0) < 0) {
        free(data);
        close(sockfd);
        return -1.0;
    }
    
    // Start timing
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    size_t total_sent = 0;
    size_t chunks_to_send = total_size / chunk_size;
    
    for (size_t i = 0; i < chunks_to_send; i++) {
        ssize_t sent = send(sockfd, data, chunk_size, 0);
        if (sent <= 0) break;
        total_sent += sent;
        
        // Check elapsed time
        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) + 
                        (end.tv_usec - start.tv_usec) / 1000000.0;
        
        if (elapsed >= duration_sec) {
            break;
        }
    }
    
    gettimeofday(&end, NULL);
    
    free(data);
    close(sockfd);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_usec - start.tv_usec) / 1000000.0;
    
    if (elapsed == 0) elapsed = 0.001;
    
    double speed_mbps = (total_sent * 8.0) / (elapsed * 1000000.0);
    
    return speed_mbps;
}
