#pragma once

#include <zephyr/net/net_ip.h>

int resolve_addr(const char *addr, uint16_t port, struct sockaddr_in *out_addr);
