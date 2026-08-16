#ifndef SERVERS_H
#define SERVERS_H

#include <stddef.h>

char **parse_server_list(
    char **buffer,
    char *country,
    size_t *host_count
);

char *remove_port(char *host);

char *find_valid_host(
    char **hosts,
    size_t host_count
);

#endif