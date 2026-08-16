#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <getopt.h>
#include <cjson/cJSON.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0
#define MAX_SECONDS 15
#define SERVER_LIST_FILE "speedtest_server_list.json"
#define BUFFER_SIZE 1024
#define LOCATION_API_URL "http://ip-api.com/json/?fields=16401"

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

double elapsed_seconds(struct timespec *start, struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec) / 1e9;
    
}

static size_t write_cb(char *contents, size_t size, size_t nmemb, void *userp)
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

void *timer(void *arg)
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

void *start_upspeed_test(void *arg)
{ // CURL *curl, char *buffer, size_t sent, size_t *sum, clock_t *end, clock_t *start, int seconds
    thread_data_t *data = (thread_data_t *)arg;
    CURL *curl = data->curl;
    char *buffer = data->buffer;
    size_t sent = data->sent;
    size_t *sum = data->sum;
    int seconds = data->seconds;
    CURLcode result;
    result; // to avoid unused variable warning :) creates another warning
    clock_gettime(CLOCK_MONOTONIC, data->start);

    struct timespec now;
    double elapsed = 0.0;
    while (elapsed < seconds)
    {
        result = curl_easy_send(curl, buffer, BUFFER_SIZE, &sent);
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

char **parse_server_list(char **buffer, char *country, size_t *host_count)
{
    FILE *fp = fopen(SERVER_LIST_FILE, "r");
    if (fp == NULL)
    {
        free(buffer);
        fprintf(stderr, "Error opening file\n");
        return NULL;
    }
    else
    {
        printf("File opened successfully\n");
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    printf("File size: %ld bytes\n", size);

    *buffer = malloc(size + 1);
    if (*buffer == NULL)
    {
        fprintf(stderr, "Error allocating memory\n");
        fclose(fp);
        return NULL;
    }
    else
    {
        printf("Memory allocated successfully\n");
    }

    fread(*buffer, 1, size, fp);
    cJSON *json = cJSON_Parse(*buffer);
    if (json == NULL)
    {
        fprintf(stderr, "Error parsing JSON\n");
        fclose(fp);
        free(buffer);
        return NULL;
    }
    else
    {
        printf("JSON parsed successfully\n");
    }

    char **hosts = NULL;
    cJSON *server = NULL;

    cJSON_ArrayForEach(server, json)
    {
        if (cJSON_GetObjectItem(server, "country") && strcmp(cJSON_GetObjectItem(server, "country")->valuestring, country) == 0)
        {
            hosts = realloc(hosts, sizeof(char *) * (*host_count + 1));
            if (hosts == NULL)
            {
                fprintf(stderr, "Error reallocating memory\n");
                fclose(fp);
                free(buffer);
                cJSON_Delete(json);
                return NULL;
            }
            hosts[*host_count] = strdup(cJSON_GetObjectItem(server, "host")->valuestring);
            (*host_count)++;
        }
    }

    fclose(fp);
    cJSON_Delete(json);
    return hosts;
}

char *remove_port(char *host)
{
    char *colon = strchr(host, ':');
    if (colon)
    {
        *colon = '\0'; // cut string at ':'
    }
    return host;
}

char *get_country(const char *url)
{
    CURL *curl = NULL;
    CURLcode result;
    char *country_str = NULL;
    struct MemoryStruct chunk;

    result = curl_global_init(CURL_GLOBAL_ALL);
    if (result != CURLE_OK)
        return NULL;

    chunk.memory = malloc(1);
    chunk.size = 0;
    curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        result = curl_easy_perform(curl);
        if (result != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(result));
        }
        else
        {
            cJSON *json = cJSON_Parse(chunk.memory);
            if (json == NULL)
            {
                fprintf(stderr, "Error parsing JSON\n");
                free(chunk.memory);
                return NULL;
            }
            cJSON *country = cJSON_GetObjectItem(json, "country");
            if (country == NULL)
            {
                fprintf(stderr, "Error getting country from JSON\n");
                cJSON_Delete(json);
                free(chunk.memory);
                return NULL;
            }
            country_str = strdup(country->valuestring);
            cJSON_Delete(json);
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    
    free(chunk.memory);

    return country_str;
}

char *find_valid_host(char **hosts, size_t host_count)
{
    char *url = NULL;
    CURL *curl = curl_easy_init();
    if (curl){
        CURLcode result;
        size_t sent;
        char buffer[BUFFER_SIZE] = "test";
        for (size_t i = 0; i < host_count; i++)
        {
            printf("Testing host %zu: %s\n", i , hosts[i]);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // set timeout to 5 seconds for each host test
            curl_easy_setopt(curl, CURLOPT_URL, hosts[i]);
            curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
            result = curl_easy_perform(curl);
            if (result == CURLE_OK)
            {
                result = curl_easy_send(curl, buffer, BUFFER_SIZE, &sent);
                if (result == CURLE_OK && sent > 0)
                {
                    url = strdup(hosts[i]);
                    curl_easy_cleanup(curl);
                    return url;
                }
            }
        }
    }
    else
    {
        fprintf(stderr, "Error initializing curl\n");
        curl_easy_cleanup(curl);
        return NULL;
    }
    curl_easy_cleanup(curl);
    fprintf(stderr, "No valid host found\n");
    return NULL;
}

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
        size_t host_count = 0;;
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

    }

    if (do_upspeed_test == TRUE)
    {
    
        size_t sent = 0;
        CURL *curl = curl_easy_init();

        if (curl)
        {
            CURLcode result;
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
            result = curl_easy_perform(curl);

            if (result == CURLE_OK)
            {
                printf("Connected, %i\n", result);
                curl_socket_t sockfd;

                result = curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sockfd);
                size_t sum = 0;
                struct timespec start, end;
                thread_data_t data;

                data.curl = curl;
                data.buffer = buffer;
                data.sent = sent;
                data.sum = &sum;
                data.start = &start;
                data.end = &end;
                data.seconds = MAX_SECONDS;

                printf("starting upspeed test\n");
                // send data 
                int seconds = MAX_SECONDS;

                pthread_t timer_t, upspeed_test_t; // setup and run multithreaded timer and upload test

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

                free(buffer);
            }
            else
            {
                fprintf(stderr, "Error: %s\n", curl_easy_strerror(result));
            }
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
        if (curl)
        {
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
    }
    
    return 0;
}
