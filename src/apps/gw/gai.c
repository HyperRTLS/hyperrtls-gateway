#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

#include "gai.h"

LOG_MODULE_REGISTER(gai);

static size_t addrinfo_len(struct zsock_addrinfo *addr) {
    size_t res = 0;
    while (addr) {
        ++res;
        addr = addr->ai_next;
    }
    return res;
}

int resolve_addr(const char *addr, uint16_t port, struct sockaddr_in *out_addr) {
    assert(addr);
    assert(out_addr);

    static const struct zsock_addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%" PRIu16, port);

    struct zsock_addrinfo *res = NULL;
    int err = zsock_getaddrinfo(addr, port_str, &hints, &res);

    if (!err) {
        size_t hits = addrinfo_len(res);
        if (hits != 1) {
            LOG_WRN("Unexpected multiple hits for given addr hints: %zu", hits);
        }

        out_addr->sin_family = AF_INET;
        out_addr->sin_port = htons(port);
        net_ipaddr_copy(&out_addr->sin_addr, &net_sin(res->ai_addr)->sin_addr);
    }
    if (res) {
        freeaddrinfo(res);
    }
    return err;
}
