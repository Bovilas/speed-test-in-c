#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <time.h>

#include <curl/curl.h>

typedef struct
{
    CURL *curl;
    char *buffer;
    size_t sent;
    size_t *sum;
    struct timespec *end;
    struct timespec *start;
    int seconds;
    struct MemoryStruct *chunk;
} thread_data_t;


struct MemoryStruct
{
    char *memory;
    size_t size;
};

double elapsed_seconds (struct timespec *start, 
                        struct timespec *end);
                        
size_t write_cb (char *contents, 
                        size_t size, 
                        size_t nmemb, 
                        void *userp);

void *timer (void *arg);

#endif // COMMON_H