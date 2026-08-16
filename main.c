#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <getopt.h>
#include <cjson/cJSON.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "location.h"
#include "servers.h"
#include "speedtest.h"

int main(int argc, char *argv[])
{
    int do_upspeed_test = FALSE;
    int do_downspeed_test = FALSE;
    int do_find_host = FALSE;
    int do_find_location = FALSE;

    const char *locationApiUrl = LOCATION_API_URL;
    char *country = NULL;
    char *url = NULL;
    char *buffer = NULL;

    int c;

    opterr = 0;

    while ((c = getopt(argc, argv, "v:l:H:")) != -1) {
        switch (c) {
        case 'v':
        printf("%s\n", optarg);
            switch (optarg[0]) {
                case 'l':
                    printf("find location test\n");
                    do_find_location = TRUE;
                    break;
                case 'h'    :
                    printf("find host test\n");
                    do_find_host = TRUE;
                    break;
                case 'u':
                    printf("upload speed test\n");
                    do_upspeed_test = TRUE;
                    break;
                case 'd':
                    printf("download speed test\n");
                    do_downspeed_test = TRUE;
                    break;
                case 'a':
                    printf("upload and download speed test\n");
                    do_upspeed_test = TRUE;
                    do_find_location = TRUE;
                    do_downspeed_test = TRUE;
                    do_find_host = TRUE;
                    break;
                default:
                    printf("unknown option: %s\n", optarg);
            }
            break;

        case 'l':
        do_find_location = FALSE;
        country = optarg;
        printf("location:%s\n", country);
            break;

        case 'H':
        do_find_host = FALSE;
        url = optarg;
        printf("host:%s\n", optarg);    
        break;

        case '?':
        printf("usage: %s -v <test> -l <location> -H <host>\n", argv[0]);
        printf("tests: l - find location, h - find host, u - upload speed test, d - download speed test, a - upload and download speed test\n");
        printf("example: %s -v l\n", argv[0]);
        printf("example: %s -v u -l Lithuania\n", argv[0]);
        printf("example: %s -v a -H speed-kaunas.telia.lt\n", argv[0]);

        default:
            break;
        }
    }

    if( (argc == 1) || (do_find_location == FALSE && do_find_host == FALSE && do_upspeed_test == FALSE && do_downspeed_test == FALSE) ) // one argument or no options provided
    {
        printf("No options provided. Running all tests by default.\n");
        do_find_location = TRUE;
        do_find_host = TRUE;
        do_upspeed_test = TRUE;
        do_downspeed_test = TRUE;
    }
    printf("do_find_location: %d\n", do_find_location);
    printf("do_find_host: %d\n", do_find_host);
    printf("do_upspeed_test: %d\n", do_upspeed_test);
    printf("do_downspeed_test: %d\n", do_downspeed_test);

    if(do_find_location == TRUE) {
        printf("Finding location...\n");
        char *country_ptr;
        country_ptr = get_country(locationApiUrl);
        printf("Country: %s\n", country_ptr);
        country = strdup(country_ptr);
        free(country_ptr);
    }

    if(do_find_host) {
        printf("Finding host...\n");
        size_t host_count = 0;
        char **hosts = parse_server_list(&buffer, country, &host_count);
        if (buffer == NULL)
        {
            fprintf(stderr, "Error parsing server list\n");
            return 1;
        }
        printf("Server list parsed successfully\n");
        printf("Country: %s\n", country);
        printf("host count: %zu\n", host_count);
        for (size_t i = 0; i < host_count; i++)
        {
            hosts[i] = remove_port(hosts[i]);
        }

        url = find_valid_host(hosts, host_count);
        if (!url)
        {
            fprintf(stderr, "Error finding valid host\n");
            return 1;
        }
        else
        {
            printf("Using host: %s\n", url);
        }
        free(country);

        for (size_t i = 0; i < host_count; i++)
        {
            free(hosts[i]);
        }
        free(hosts);
        
    }

    if (do_upspeed_test == TRUE)
{
    size_t sent = 0;
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        fprintf(stderr, "Error initializing curl for upload test\n");
    }
    else
    {
        CURLcode result;
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
        result = curl_easy_perform(curl);

        if (result != CURLE_OK)
        {
            fprintf(stderr, "Error: %s\n", curl_easy_strerror(result));
        }
        else
        {
            printf("Connected, %i\n", result);
            curl_socket_t sockfd;
            result = curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sockfd);

            size_t sum = 0;
            struct timespec start, end;
            thread_data_t data = {
                .curl = curl, .buffer = buffer, .sent = sent, .sum = &sum,
                .start = &start, .end = &end, .seconds = MAX_SECONDS
            };

            printf("starting upspeed test\n");
            int seconds = MAX_SECONDS;
            pthread_t timer_t, upspeed_test_t;

            pthread_create(&timer_t, NULL, timer, (void *)&seconds);
            pthread_create(&upspeed_test_t, NULL, start_upspeed_test, (void *)&data);
            pthread_join(timer_t, NULL);
            pthread_join(upspeed_test_t, NULL);

            printf("\nSent %llu bytes\n", sum);
            double total_time = elapsed_seconds(&start, &end);
            printf("Execution time: %.6f seconds\n", total_time);
            printf("Upload test completed, results:\n");
            printf("at %f Mbps\n", (sum / total_time) / 125000.0);
            printf("at %f MB/s\n", (sum / total_time) / 125000.0 / 8.0);
        }
        curl_easy_cleanup(curl);
    }
}

    if (do_downspeed_test == TRUE)
    {
        CURL *curl = NULL;
        CURLcode result;

        struct MemoryStruct chunk;

        result = curl_global_init(CURL_GLOBAL_ALL);
        if (result != CURLE_OK)
            return (int)result;

        chunk.memory = malloc(1);
        chunk.size = 0;

        curl = curl_easy_init();
        if (!curl)
        {
            fprintf(stderr, "Error initializing curl for download test\n");
            free(chunk.memory);
            curl_global_cleanup();
            return 1;
        }

        fprintf(stdout, "Starting download speed test...\n");
        curl_easy_setopt(curl, CURLOPT_URL, "https://speed-kaunas.telia.lt/speedtest/random1000x1000.jpg"); // ~500 kB

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        size_t sum = 0;
        struct timespec start, end;
        thread_data_t data;

        data.curl = curl;
        data.sum = &sum;
        data.start = &start;
        data.end = &end;
        data.seconds = MAX_SECONDS;
        int seconds = MAX_SECONDS;
        data.chunk = &chunk;

        pthread_t timer_t, downspeed_test_t; // setup and run multithreaded timer and download test

        pthread_create(&timer_t, NULL, timer, (void *)&seconds);
        pthread_create(&downspeed_test_t, NULL, start_downspeed_test, (void *)&data);

        pthread_join(timer_t, NULL);
        pthread_join(downspeed_test_t, NULL);

        printf("\nRecieved %llu bytes\n", sum);
        double total_time = elapsed_seconds(&start, &end);
        printf("Execution time: %.6f seconds\n", total_time);
        printf("Download test completed, results:\n");
        printf("at %f Mbps\n", (sum / total_time) / 125000.0);
        printf("at %f MB/s\n", (sum / total_time) / 125000.0 / 8.0);

        // cleanup curl stuff
        curl_easy_cleanup(curl);
        free(chunk.memory);
        curl_global_cleanup();
        
    }
    free(buffer);
    free(url);
    
    return 0;
}