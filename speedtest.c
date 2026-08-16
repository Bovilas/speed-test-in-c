#include "speedtest.h"
#include "common.h"
#include "config.h"

#include <stdio.h>
#include <curl/curl.h>

void *start_upspeed_test(void *arg)
{ // CURL *curl, char *buffer, size_t sent, size_t *sum, clock_t *end, clock_t *start, int seconds
    thread_data_t *data = (thread_data_t *)arg;
    CURL *curl = data->curl;
    char *buffer = data->buffer;
    size_t sent = data->sent;
    size_t *sum = data->sum;
    int seconds = data->seconds;
    CURLcode result;
    clock_gettime(CLOCK_MONOTONIC, data->start);

    struct timespec now;
    double elapsed = 0.0;
    while (elapsed < seconds)
    {
        result = curl_easy_send(curl, buffer, BUFFER_SIZE, &sent);
        if (result != CURLE_OK) {}; // :)
    
        *sum += sent;

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = elapsed_seconds(data->start, &now);
    }

    clock_gettime(CLOCK_MONOTONIC, data->end);
    return NULL;
}

void *start_downspeed_test(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    CURL *curl = data->curl;
    size_t *sum = data->sum;
    struct timespec *start = data->start;
    int seconds = data->seconds;
    CURLcode result;
    struct MemoryStruct *chunk = data->chunk;

    struct timespec now;
    double elapsed = 0.0;
    clock_gettime(CLOCK_MONOTONIC, data->start);
    clock_gettime(CLOCK_MONOTONIC, &now);

    while (elapsed < seconds) {
        result = curl_easy_perform(curl);

        if (result != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(result));
        }
        else
        { 
            *sum += (unsigned long)chunk->size;
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = elapsed_seconds(start, &now);
    }

    clock_gettime(CLOCK_MONOTONIC, data->end);
    return NULL;
}