#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "../include/network.h"
#include "../include/speedtest.h"

// Thread data structure for parallel downloads
typedef struct {
    const char *host;
    int port;
    int duration_sec;
    size_t bytes_downloaded;
    int thread_id;
    int success;
} download_thread_data_t;

// Thread data structure for parallel uploads
typedef struct {
    const char *host;
    int port;
    int duration_sec;
    size_t bytes_uploaded;
    int thread_id;
    int success;
} upload_thread_data_t;

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

// Worker thread for downloading - OPTIMIZED
void* download_worker(void* arg) {
    download_thread_data_t *data = (download_thread_data_t*)arg;
    
    fprintf(stderr, "[DEBUG] Thread %d: Starting download\n", data->thread_id);
    
    int sockfd = create_tcp_socket();
    if (sockfd < 0) {
        fprintf(stderr, "[ERROR] Thread %d: Failed to create socket\n", data->thread_id);
        data->success = 0;
        return NULL;
    }
    
    // AGGRESSIVE OPTIMIZATIONS
    int flag = 1;
    
    // Disable Nagle's algorithm for lower latency
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    
    // Massive receive buffer - 2MB
    int rcvbuf = 2097152;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    
    // Enable TCP Quick ACK (Linux-specific)
    #ifdef TCP_QUICKACK
    setsockopt(sockfd, IPPROTO_TCP, TCP_QUICKACK, &flag, sizeof(int));
    #endif
    
    // Set high priority
    int priority = 6;
    setsockopt(sockfd, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority));
    
    if (connect_to_server(sockfd, data->host, data->port) < 0) {
        fprintf(stderr, "[ERROR] Thread %d: Failed to connect\n", data->thread_id);
        close(sockfd);
        data->success = 0;
        return NULL;
    }
    
    // Send HTTP request with aggressive headers
        char request[1024];
        snprintf(request, sizeof(request),
                "GET /100MB.bin HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: LatencyLabs/1.0\r\n"
                "Accept-Encoding: identity\r\n"
                "Connection: close\r\n"
                "\r\n", data->host);


    
    if (send(sockfd, request, strlen(request), 0) < 0) {
        fprintf(stderr, "[ERROR] Thread %d: Failed to send request\n", data->thread_id);
        close(sockfd);
        data->success = 0;
        return NULL;
    }
    
    // LARGER BUFFER - 256KB for faster downloads
    char *buffer = malloc(262144);
    if (!buffer) {
        fprintf(stderr, "[ERROR] Thread %d: Failed to allocate buffer\n", data->thread_id);
        close(sockfd);
        data->success = 0;
        return NULL;
    }
    
    int headers_skipped = 0;
    size_t total_bytes = 0;
    
    // Skip HTTP headers
    while (!headers_skipped) {
        ssize_t bytes = recv(sockfd, buffer, 262144, 0);
        if (bytes <= 0) {
            fprintf(stderr, "[ERROR] Thread %d: Failed to receive headers\n", data->thread_id);
            free(buffer);
            close(sockfd);
            data->success = 0;
            return NULL;
        }
        
        // Look for end of headers
        for (int i = 0; i < bytes - 3; i++) {
            if (buffer[i] == '\r' && buffer[i+1] == '\n' && 
                buffer[i+2] == '\r' && buffer[i+3] == '\n') {
                headers_skipped = 1;
                total_bytes = bytes - (i + 4);
                break;
            }
        }
    }
    
    fprintf(stderr, "[DEBUG] Thread %d: Headers skipped, starting download\n", data->thread_id);
    
    // Start timing
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    // Download data with aggressive recv
    int recv_count = 0;
    while (1) {
        // Use MSG_WAITALL for more efficient receiving
        ssize_t bytes = recv(sockfd, buffer, 262144, 0);
        if (bytes <= 0) break;
        
        total_bytes += bytes;
        recv_count++;
        
        // Check elapsed time every 50 receives (not every time for performance)
        if (recv_count % 50 == 0) {
            gettimeofday(&end, NULL);
            double elapsed = (end.tv_sec - start.tv_sec) + 
                            (end.tv_usec - start.tv_usec) / 1000000.0;
            
            if (elapsed >= data->duration_sec) break;
        }
    }
    
    gettimeofday(&end, NULL);
    free(buffer);
    close(sockfd);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_usec - start.tv_usec) / 1000000.0;
    double speed = (total_bytes * 8.0) / (elapsed * 1000000.0);
    
    fprintf(stderr, "[DEBUG] Thread %d: Downloaded %zu bytes in %.2f sec (%.2f Mbps, %d recvs)\n", 
            data->thread_id, total_bytes, elapsed, speed, recv_count);
    
    data->bytes_downloaded = total_bytes;
    data->success = 1;
    return NULL;
}

double test_download_speed(const char* host, int port, int duration_sec) {
    const int NUM_THREADS = 8;  // Increased from 4 to 8 for more parallelism
    
    fprintf(stderr, "[DEBUG] Starting multi-threaded download test (%d connections)\n", NUM_THREADS);
    
    pthread_t threads[NUM_THREADS];
    download_thread_data_t thread_data[NUM_THREADS];
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    // Create all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].host = host;
        thread_data[i].port = port;
        thread_data[i].duration_sec = duration_sec;
        thread_data[i].bytes_downloaded = 0;
        thread_data[i].thread_id = i;
        thread_data[i].success = 0;
        
        if (pthread_create(&threads[i], NULL, download_worker, &thread_data[i]) != 0) {
            fprintf(stderr, "[ERROR] Failed to create thread %d\n", i);
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    gettimeofday(&end, NULL);
    
    // Sum up all downloads
    size_t total_bytes = 0;
    int successful_threads = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        if (thread_data[i].success) {
            total_bytes += thread_data[i].bytes_downloaded;
            successful_threads++;
        }
    }
    
    if (successful_threads == 0) {
        fprintf(stderr, "[ERROR] All download threads failed\n");
        return -1.0;
    }
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_usec - start.tv_usec) / 1000000.0;
    
    if (elapsed < 0.001) elapsed = 0.001;
    
    double speed_mbps = (total_bytes * 8.0) / (elapsed * 1000000.0);
    
    fprintf(stderr, "[DEBUG] Download complete: %d/%d threads successful\n", 
            successful_threads, NUM_THREADS);
    fprintf(stderr, "[DEBUG] Total downloaded: %zu bytes (%.2f MB) in %.2f seconds\n", 
            total_bytes, total_bytes / (1024.0 * 1024.0), elapsed);
    fprintf(stderr, "[DEBUG] Combined speed: %.2f Mbps\n", speed_mbps);
    
    return speed_mbps;
}

// Worker thread for uploading - OPTIMIZED
void* upload_worker(void* arg) {
    upload_thread_data_t *data = (upload_thread_data_t*)arg;
    
    fprintf(stderr, "[DEBUG] Thread %d: Starting upload\n", data->thread_id);
    
    int sockfd = create_tcp_socket();
    if (sockfd < 0) {
        fprintf(stderr, "[ERROR] Thread %d: Failed to create socket\n", data->thread_id);
        data->success = 0;
        return NULL;
    }
    
    // AGGRESSIVE OPTIMIZATIONS
    int flag = 1;
    
    // Disable Nagle's algorithm
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    
    // Massive send buffer - 2MB
    int sndbuf = 2097152;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    
    // Set high priority
    int priority = 6;
    setsockopt(sockfd, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority));
    
    if (connect_to_server(sockfd, data->host, data->port) < 0) {
        fprintf(stderr, "[ERROR] Thread %d: Failed to connect\n", data->thread_id);
        close(sockfd);
        data->success = 0;
        return NULL;
    }
    
    // LARGER BUFFER - 256KB chunks
    size_t chunk_size = 262144;
    char *buffer = malloc(chunk_size);
    if (!buffer) {
        fprintf(stderr, "[ERROR] Thread %d: Failed to allocate buffer\n", data->thread_id);
        close(sockfd);
        data->success = 0;
        return NULL;
    }
    memset(buffer, 'A', chunk_size);
    
    // Send POST header
    size_t total_size = chunk_size * 100;
    char header[512];
    snprintf(header, sizeof(header),
             "POST /upload HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/octet-stream\r\n"
             "Content-Length: %zu\r\n"
             "\r\n", data->host, total_size);
    
    if (send(sockfd, header, strlen(header), 0) < 0) {
        fprintf(stderr, "[ERROR] Thread %d: Failed to send header\n", data->thread_id);
        free(buffer);
        close(sockfd);
        data->success = 0;
        return NULL;
    }
    
    // Start timing
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    size_t total_sent = 0;
    int send_count = 0;
    
    while (1) {
        ssize_t sent = send(sockfd, buffer, chunk_size, MSG_NOSIGNAL);
        if (sent <= 0) break;
        
        total_sent += sent;
        send_count++;
        
        // Check elapsed time every 20 sends
        if (send_count % 20 == 0) {
            gettimeofday(&end, NULL);
            double elapsed = (end.tv_sec - start.tv_sec) + 
                            (end.tv_usec - start.tv_usec) / 1000000.0;
            
            if (elapsed >= data->duration_sec) break;
        }
    }
    
    gettimeofday(&end, NULL);
    
    free(buffer);
    close(sockfd);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_usec - start.tv_usec) / 1000000.0;
    double speed = (total_sent * 8.0) / (elapsed * 1000000.0);
    
    fprintf(stderr, "[DEBUG] Thread %d: Uploaded %zu bytes in %.2f sec (%.2f Mbps, %d sends)\n", 
            data->thread_id, total_sent, elapsed, speed, send_count);
    
    data->bytes_uploaded = total_sent;
    data->success = 1;
    return NULL;
}

double test_upload_speed(const char* host, int port, int duration_sec) {
    const int NUM_THREADS = 8;  // Increased from 4 to 8
    
    fprintf(stderr, "[DEBUG] Starting multi-threaded upload test (%d connections)\n", NUM_THREADS);
    
    pthread_t threads[NUM_THREADS];
    upload_thread_data_t thread_data[NUM_THREADS];
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    // Create all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].host = host;
        thread_data[i].port = port;
        thread_data[i].duration_sec = duration_sec;
        thread_data[i].bytes_uploaded = 0;
        thread_data[i].thread_id = i;
        thread_data[i].success = 0;
        
        if (pthread_create(&threads[i], NULL, upload_worker, &thread_data[i]) != 0) {
            fprintf(stderr, "[ERROR] Failed to create thread %d\n", i);
        }
    }
    
    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    gettimeofday(&end, NULL);
    
    // Sum up all uploads
    size_t total_bytes = 0;
    int successful_threads = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        if (thread_data[i].success) {
            total_bytes += thread_data[i].bytes_uploaded;
            successful_threads++;
        }
    }
    
    if (successful_threads == 0) {
        fprintf(stderr, "[ERROR] All upload threads failed\n");
        return -1.0;
    }
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_usec - start.tv_usec) / 1000000.0;
    
    if (elapsed < 0.001) elapsed = 0.001;
    
    double speed_mbps = (total_bytes * 8.0) / (elapsed * 1000000.0);
    
    fprintf(stderr, "[DEBUG] Upload complete: %d/%d threads successful\n", 
            successful_threads, NUM_THREADS);
    fprintf(stderr, "[DEBUG] Total uploaded: %zu bytes (%.2f MB) in %.2f seconds\n", 
            total_bytes, total_bytes / (1024.0 * 1024.0), elapsed);
    fprintf(stderr, "[DEBUG] Combined speed: %.2f Mbps\n", speed_mbps);
    
    return speed_mbps;
}
