#pragma once

#include <stddef.h>
#include <stdint.h>

typedef void gw_mqtt_client_pub_handler_t(void);

struct gw_mqtt_client_config {
    const char *server_addr;
    uint16_t server_port;
    const char *username;
    const char *password;
    const char *const *topics;
    size_t topics_len;
    gw_mqtt_client_pub_handler_t *pub_handler;
};

int gw_mqtt_client_try_publishing(const char *topic, const uint8_t *message, size_t len);
void gw_mqtt_client_run(const struct gw_mqtt_client_config *config);
