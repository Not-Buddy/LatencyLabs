#include <stdio.h>
#include <stdlib.h>
#include "../include/speedtest.h"
#include "../include/network.h"

int main(int argc, char *argv[]) {
    // CacheFly CDN - proven reliable global CDN with test files
    const char *download_host = "ash-speed.hetzner.com";
    const char *upload_host = "httpbin.org";
    const char *custom_host = NULL;
    int test_port = 80;
    int test_duration = 5;
    
    if (argc > 1) {
        custom_host = argv[1];
    }
    
    printf("========================================\n");
    printf("      LatencyLabs Speed Test\n");
    printf("========================================\n\n");
    
    // Test latency
    const char *latency_host = custom_host ? custom_host : "google.com";
    printf("Testing latency to %s...\n", latency_host);
    double latency = test_latency(latency_host);
    if (latency < 0) {
        printf("✗ Latency test failed\n\n");
    } else {
        printf("✓ Latency: %.2f ms\n\n", latency);
    }
    
    // Download test
    printf("Testing download speed from %s (duration: %d seconds)...\n", 
           download_host, test_duration);
    double download_speed = test_download_speed(download_host, test_port, test_duration);
    if (download_speed < 0) {
        printf("✗ Download test failed\n\n");
    } else {
        printf("✓ Download Speed: %.2f Mbps\n\n", download_speed);
    }
    
    // Upload test
    printf("Testing upload speed to %s (duration: %d seconds)...\n", 
           upload_host, test_duration);
    double upload_speed = test_upload_speed(upload_host, test_port, test_duration);
    if (upload_speed < 0) {
        printf("✗ Upload test failed\n\n");
    } else {
        printf("✓ Upload Speed: %.2f Mbps\n\n", upload_speed);
    }
    
    printf("========================================\n");
    printf("Test completed!\n");
    
    return 0;
}
