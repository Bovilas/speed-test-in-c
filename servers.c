#include "servers.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <cjson/cJSON.h>

char **parse_server_list(char **buffer, char *country, size_t *host_count)
{
    FILE *fp = fopen(SERVER_LIST_FILE, "r");
    if (fp == NULL)
    {
        free(buffer);
        fprintf(stderr, "Error opening file\n");
        return NULL;
    }
    
    printf("File opened successfully\n");
    

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
    printf("Memory allocated successfully\n");
    

    fread(*buffer, 1, size, fp);
    cJSON *json = cJSON_Parse(*buffer);
    if (json == NULL)
    {
        fprintf(stderr, "Error parsing JSON\n");
        fclose(fp);
        free(buffer);
        return NULL;
    }

    printf("JSON parsed successfully\n");
    

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
            printf("Found valid host %zu: %s\n", *host_count - 1, hosts[*host_count - 1]);
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

char *find_valid_host(char **hosts, size_t host_count)
{
    char *url = NULL;
    CURL *curl = curl_easy_init();
    if (!curl) 
    {
        fprintf(stderr, "Error initializing curl\n");
        return NULL;
    }

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
    
    fprintf(stderr, "Error initializing curl\n");
    curl_easy_cleanup(curl);
    return NULL;
    
    curl_easy_cleanup(curl);
    fprintf(stderr, "No valid host found\n");
    return NULL;
}