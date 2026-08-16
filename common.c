#include "common.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


double elapsed_seconds(struct timespec *start, struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec) / 1e9;
}

size_t write_cb(char *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr)
    {
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

void *timer (void *arg)
{
    int seconds = *(int *)arg;
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    double elapsed = 0.0;
    while (elapsed < seconds)
    {
        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = elapsed_seconds(&start, &now);
        printf("\rtime elapsed: %0.2f/%d seconds", elapsed, MAX_SECONDS);
        fflush(stdout);
        usleep(100000);

    }
    return NULL;
}
