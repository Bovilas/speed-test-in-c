#include "location.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <cjson/cJSON.h>

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
    if (!curl)
    {
        fprintf(stderr, "Error initializing curl\n");
        free(chunk.memory);
        curl_global_cleanup();
        return NULL;
    }

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
    curl_global_cleanup();
    free(chunk.memory);

    return country_str;
}