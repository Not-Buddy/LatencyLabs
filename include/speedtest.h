#ifndef SPEEDTEST_H
#define SPEEDTEST_H

double test_latency(const char* host);
double test_download_speed(const char* host, int port, int duration_sec);
double test_upload_speed(const char* host, int port, int duration_sec);

#endif
