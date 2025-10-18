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
    fprintf(stderr, "[DEBUG] Starting download test to %s:%d\n", host, port);
    
    int sockfd = create_tcp_socket();
    if (sockfd < 0) {
        fprintf(stderr, "[ERROR] Failed to create socket\n");
        return -1.0;
    }
    
    fprintf(stderr, "[DEBUG] Connecting to server...\n");
    if (connect_to_server(sockfd, host, port) < 0) {
        fprintf(stderr, "[ERROR] Failed to connect to server\n");
        close(sockfd);
        return -1.0;
    }
    fprintf(stderr, "[DEBUG] Connected successfully\n");
    
    // Set socket receive timeout
    struct timeval timeout;
    timeout.tv_sec = 15;
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
    
    fprintf(stderr, "[DEBUG] Sending HTTP request...\n");
    if (send(sockfd, request, strlen(request), 0) < 0) {
        fprintf(stderr, "[ERROR] Failed to send HTTP request\n");
        close(sockfd);
        return -1.0;
    }
    fprintf(stderr, "[DEBUG] HTTP request sent\n");
    
    char buffer[65536];  // 64KB buffer for faster downloads
    size_t total_bytes = 0;
    int headers_skipped = 0;
    int header_recv_count = 0;
    
    // Skip HTTP headers first (DON'T start timing yet)
    fprintf(stderr, "[DEBUG] Waiting for HTTP headers...\n");
    while (!headers_skipped) {
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        header_recv_count++;
        
        if (bytes <= 0) {
            fprintf(stderr, "[ERROR] Failed to receive data (recv returned %zd)\n", bytes);
            close(sockfd);
            return -1.0;
        }
        
        fprintf(stderr, "[DEBUG] Received %zd bytes in header recv #%d\n", bytes, header_recv_count);
        
        // Look for \r\n\r\n (end of headers)
        for (int i = 0; i < bytes - 3; i++) {
            if (buffer[i] == '\r' && buffer[i+1] == '\n' && 
                buffer[i+2] == '\r' && buffer[i+3] == '\n') {
                headers_skipped = 1;
                // Count data after headers in this first buffer
                total_bytes = bytes - (i + 4);
                fprintf(stderr, "[DEBUG] Headers skipped! Found at position %d, %zu bytes of data after headers\n", 
                        i, total_bytes);
                break;
            }
        }
        
        // If we didn't find header end in this buffer, keep looping
        if (!headers_skipped) {
            fprintf(stderr, "[DEBUG] Header end not found yet, continuing...\n");
        }
    }
    
    // NOW start timing - after headers are skipped and we have actual data
    struct timeval start, end;
    gettimeofday(&start, NULL);
    fprintf(stderr, "[DEBUG] Starting download timer, initial bytes: %zu\n", total_bytes);
    
    int recv_count = 0;
    // Download for the specified duration
    while (1) {
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        recv_count++;
        
        if (bytes <= 0) {
            fprintf(stderr, "[DEBUG] Connection closed or error (recv #%d returned %zd)\n", recv_count, bytes);
            break;
        }
        
        total_bytes += bytes;
        
        // Log progress every 100 receives
        if (recv_count % 100 == 0) {
            gettimeofday(&end, NULL);
            double current_elapsed = (end.tv_sec - start.tv_sec) + 
                                    (end.tv_usec - start.tv_usec) / 1000000.0;
            double current_speed = (total_bytes * 8.0) / (current_elapsed * 1000000.0);
            
            fprintf(stderr, "[DEBUG] Progress: %d receives, %zu total bytes (%.2f MB, %.2f Mbps)\n",
                    recv_count, total_bytes, total_bytes / (1024.0 * 1024.0), current_speed);
        }

        
        // Check elapsed time
        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) + 
                        (end.tv_usec - start.tv_usec) / 1000000.0;
        
        if (elapsed >= duration_sec) {
            fprintf(stderr, "[DEBUG] Duration reached: %.2f seconds\n", elapsed);
            break;
        }
    }
    
    gettimeofday(&end, NULL);
    close(sockfd);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_usec - start.tv_usec) / 1000000.0;
    
    fprintf(stderr, "[DEBUG] Download complete: %zu bytes in %.2f seconds (%d receives)\n", 
            total_bytes, elapsed, recv_count);
    
    if (elapsed < 0.001) elapsed = 0.001;
    
    // Calculate speed in Mbps
    double speed_mbps = (total_bytes * 8.0) / (elapsed * 1000000.0);
    
    fprintf(stderr, "[DEBUG] Calculated speed: %.2f Mbps\n", speed_mbps);
    
    return speed_mbps;
}


double test_upload_speed(const char* host, int port, int duration_sec) {
    fprintf(stderr, "[DEBUG] Starting upload test to %s:%d\n", host, port);
    
    int sockfd = create_tcp_socket();
    if (sockfd < 0) {
        fprintf(stderr, "[ERROR] Failed to create socket\n");
        return -1.0;
    }
    
    fprintf(stderr, "[DEBUG] Connecting to server...\n");
    if (connect_to_server(sockfd, host, port) < 0) {
        fprintf(stderr, "[ERROR] Failed to connect to server\n");
        close(sockfd);
        return -1.0;
    }
    fprintf(stderr, "[DEBUG] Connected successfully\n");
    
    // Prepare upload data
    size_t chunk_size = 65536;  // 64KB chunks
    fprintf(stderr, "[DEBUG] Allocating %zu bytes for upload buffer\n", chunk_size);
    char *data = malloc(chunk_size);
    if (!data) {
        fprintf(stderr, "[ERROR] Failed to allocate memory\n");
        close(sockfd);
        return -1.0;
    }
    memset(data, 'A', chunk_size);
    fprintf(stderr, "[DEBUG] Upload buffer allocated and filled\n");
    
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
    
    fprintf(stderr, "[DEBUG] Sending POST header (Content-Length: %zu)...\n", total_size);
    if (send(sockfd, header, strlen(header), 0) < 0) {
        fprintf(stderr, "[ERROR] Failed to send POST header\n");
        free(data);
        close(sockfd);
        return -1.0;
    }
    fprintf(stderr, "[DEBUG] POST header sent\n");
    
    // Start timing
    struct timeval start, end;
    gettimeofday(&start, NULL);
    fprintf(stderr, "[DEBUG] Starting upload timer\n");
    
    size_t total_sent = 0;
    size_t chunks_to_send = total_size / chunk_size;
    
    for (size_t i = 0; i < chunks_to_send; i++) {
        ssize_t sent = send(sockfd, data, chunk_size, 0);
        if (sent <= 0) {
            fprintf(stderr, "[ERROR] Send failed at chunk %zu (sent: %zd)\n", i, sent);
            break;
        }
        total_sent += sent;
        
        // Log progress every 10 chunks
        if ((i + 1) % 10 == 0) {
            gettimeofday(&end, NULL);
            double current_elapsed = (end.tv_sec - start.tv_sec) + 
                                    (end.tv_usec - start.tv_usec) / 1000000.0;
            double current_speed = (total_sent * 8.0) / (current_elapsed * 1000000.0);
            
            fprintf(stderr, "[DEBUG] Progress: Sent chunk %zu/%zu (%.2f MB, %.2f Mbps)\n",
                    i + 1, chunks_to_send, total_sent / (1024.0 * 1024.0), current_speed);
        }

        
        // Check elapsed time
        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) + 
                        (end.tv_usec - start.tv_usec) / 1000000.0;
        
        if (elapsed >= duration_sec) {
            fprintf(stderr, "[DEBUG] Duration reached: %.2f seconds\n", elapsed);
            break;
        }
    }
    
    gettimeofday(&end, NULL);
    
    free(data);
    close(sockfd);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_usec - start.tv_usec) / 1000000.0;
    
    fprintf(stderr, "[DEBUG] Upload complete: %zu bytes in %.2f seconds\n", 
            total_sent, elapsed);
    
    if (elapsed == 0) elapsed = 0.001;
    
    double speed_mbps = (total_sent * 8.0) / (elapsed * 1000000.0);
    
    fprintf(stderr, "[DEBUG] Calculated speed: %.2f Mbps\n", speed_mbps);
    
    return speed_mbps;
}
