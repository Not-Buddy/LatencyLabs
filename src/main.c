#include <stdio.h>
#include <stdlib.h>
#include "../include/speedtest.h"
#include "../include/network.h"

int main(int argc, char *argv[]) {
    const char *test_host = "speedtest.net";
    int test_port = 80;
    int test_duration = 5;
    
    // Allow custom host from command line
    if (argc > 1) {
        test_host = argv[1];
    }
    
    printf("========================================\n");
    printf("      LatencyLabs Speed Test\n");
    printf("========================================\n\n");
    
    // Test latency
    printf("Testing latency to %s...\n", test_host);
    double latency = test_latency(test_host);
    if (latency < 0) {
        printf("✗ Latency test failed\n\n");
    } else {
        printf("✓ Latency: %.2f ms\n\n", latency);
    }
    
    // Test download speed
    printf("Testing download speed (duration: %d seconds)...\n", test_duration);
    double download_speed = test_download_speed(test_host, test_port, test_duration);
    printf("✓ Download Speed: %.2f Mbps\n\n", download_speed);
    
    // Test upload speed
    printf("Testing upload speed (duration: %d seconds)...\n", test_duration);
    double upload_speed = test_upload_speed(test_host, test_port, test_duration);
    printf("✓ Upload Speed: %.2f Mbps\n\n", upload_speed);
    
    printf("========================================\n");
    printf("Test completed!\n");
    
    return 0;
}
