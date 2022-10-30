#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

#include "mqtt.h"
#include "gai.h"

LOG_MODULE_REGISTER(mqtt);

#define FATAL_ERROR INT_MIN

// TODO: make following stubs configurable with the shell
#define SERVER_ADDR "test.mosquitto.org"
#define SERVER_PORT 1883
#define RETRY_PERIOD K_SECONDS(15)
#define USERNAME "lubiepapaja2137"
#define PASSWORD "pass"
#define BUF_SIZE 2048
#define CONNECT_TIMEOUT_MS 5000
#define CLIENT_ID_MAXLEN 16
#define GW_CONFIG_TOPIC "gw/%s/config"
#define LOC_PUSH_TOPIC "tag/%s/location"
#define BATCH_MAX 32

struct client_wrapper;
typedef void evt_cb_handler_t(struct client_wrapper *wrapper, const struct mqtt_evt *evt);

struct client_wrapper {
    struct mqtt_client client;
    struct mqtt_utf8 username;
    struct mqtt_utf8 password;
    struct sockaddr_in addr;
    uint8_t rx_buffer[BUF_SIZE];
    uint8_t tx_buffer[BUF_SIZE];
    char gw_config_topic[sizeof(GW_CONFIG_TOPIC) + CLIENT_ID_MAXLEN - (sizeof("%s") - 1)];
    struct zsock_pollfd poll_fd;

    evt_cb_handler_t *overrided_evt_cb_handler;
    // TODO: reverse evt_err logic?
    int evt_err;
};

static void evt_cb_handler_connack(struct client_wrapper *wrapper, const struct mqtt_evt *evt) {
    if (evt->type != MQTT_EVT_CONNACK) {
        LOG_WRN("Unexpected packet type");
        wrapper->evt_err = -1;
        return;
    }
    if (evt->result) {
        LOG_WRN("Connect failed: %d", evt->result);
        wrapper->evt_err = -1;
        return;
    }
    LOG_INF("Server return code: %d", (int) evt->param.connack.return_code);
    LOG_INF("evt_cv_handler_connack - success");
}

static void evt_cb_handler_default(struct client_wrapper *wrapper, const struct mqtt_evt *evt) {
    // TODO: more sophisticated event handling
    LOG_INF("Received evt %d", (int) evt->type);

    if (evt->type != MQTT_EVT_PUBLISH) {
        return;
    }

    uint32_t len = evt->param.publish.message.payload.len;
    LOG_INF("Received %" PRIu32 " bytes of payload", len);

    while (len) {
        char buffer[BATCH_MAX + 1] = { 0 };
        uint32_t batch_size = len > BATCH_MAX ? BATCH_MAX : len;
        len -= batch_size;
        int res = mqtt_readall_publish_payload(&wrapper->client, buffer, sizeof(buffer) - 1);
        LOG_INF("res %d, payload: %s", res, buffer);
    }
}

static void mqtt_evt_cb(struct mqtt_client *const client,
                        const struct mqtt_evt *evt) {
    struct client_wrapper *wrapper = CONTAINER_OF(client, struct client_wrapper, client);

    assert(!wrapper->evt_err);
    if (!wrapper->overrided_evt_cb_handler) {
        evt_cb_handler_default(wrapper, evt);
        return;
    }
    wrapper->overrided_evt_cb_handler(wrapper, evt);
}

static void init_mqtt_client(struct client_wrapper *wrapper) {
    assert(wrapper);

    wrapper->username.utf8 = USERNAME;
    wrapper->username.size = sizeof(USERNAME) - 1;
    wrapper->password.utf8 = PASSWORD;
    wrapper->password.size = sizeof(PASSWORD) - 1;
    snprintf(wrapper->gw_config_topic, sizeof(wrapper->gw_config_topic), GW_CONFIG_TOPIC, USERNAME);

    mqtt_client_init(&wrapper->client);

    wrapper->client.user_name = &wrapper->username;
    wrapper->client.password = &wrapper->password;
    wrapper->client.client_id = wrapper->username;

    wrapper->client.evt_cb = mqtt_evt_cb;
    wrapper->client.broker = &wrapper->addr;
    wrapper->client.protocol_version = MQTT_VERSION_3_1_1;
    wrapper->client.transport.type = MQTT_TRANSPORT_NON_SECURE;
    wrapper->client.clean_session = true;
    wrapper->client.rx_buf = wrapper->rx_buffer;
    wrapper->client.rx_buf_size = sizeof(wrapper->rx_buffer);
    wrapper->client.tx_buf = wrapper->tx_buffer;
    wrapper->client.tx_buf_size = sizeof(wrapper->tx_buffer);

    wrapper->overrided_evt_cb_handler = NULL;
}

static int configure_mqtt_addr(struct client_wrapper *wrapper) {
    int res = resolve_addr(SERVER_ADDR, SERVER_PORT, &wrapper->addr);
    if (!res) {
        wrapper->client.broker = &wrapper->addr;
    }
    return res;
}

static void init_poll_fd(struct client_wrapper *wrapper) {
    wrapper->poll_fd.fd = wrapper->client.transport.tcp.sock;
    wrapper->poll_fd.events = ZSOCK_POLLIN;
}

static int do_poll(struct client_wrapper *wrapper, int wait_ms) {
    return zsock_poll(&wrapper->poll_fd, 1, wait_ms);
}

static int do_input(struct client_wrapper *wrapper, evt_cb_handler_t *evt_cb_handler) {
    wrapper->overrided_evt_cb_handler = evt_cb_handler;
    wrapper->evt_err = 0;

    int res = mqtt_input(&wrapper->client);
    if (res || wrapper->evt_err) {
        LOG_WRN("do_input failed with res: %d, evt_err: %d", res, wrapper->evt_err);
    }
    wrapper->overrided_evt_cb_handler = NULL;
    return res ? res : wrapper->evt_err;
}

static int do_connect(struct client_wrapper *wrapper) {
    int res = configure_mqtt_addr(wrapper);
    if (res) {
        LOG_WRN("DNS resolution failed, assuming a transient error");
        return -1;
    }

    res = mqtt_connect(&wrapper->client);
    if (res) {
        LOG_WRN("Connect failed, assuming a transient error");
        return -1;
    }
    LOG_INF("MQTT client socket connected");
    init_poll_fd(wrapper);

    res = do_poll(wrapper, CONNECT_TIMEOUT_MS);
    if (res <= 0 || wrapper->poll_fd.revents != ZSOCK_POLLIN) {
        LOG_WRN("Poll failed with res: %d, revents: %hd", res, wrapper->poll_fd.revents);
        goto abort;
    }

    res = do_input(wrapper, evt_cb_handler_connack);
    if (res) {
        goto abort;
    }

    return 0;

abort:;
    int abort_res = mqtt_abort(&wrapper->client);
    if (abort_res) {
        LOG_WRN("abort failed with res: %d", abort_res);
    }
    return res;
}

static int run_cycle(void) {
    static struct client_wrapper wrapper;
    init_mqtt_client(&wrapper);
    int res = do_connect(&wrapper);
    if (res) {
        return res == FATAL_ERROR ? FATAL_ERROR : 0;
    }

    LOG_INF("Connect with res: %d", res);

    while (true) {
        res = do_poll(&wrapper, mqtt_keepalive_time_left(&wrapper.client));
        LOG_INF("Poll res: %d, revents: %hd", res, wrapper.poll_fd.revents);
        if (res < 0) {
            LOG_WRN("Poll failed: %d", res);
            break;
        }
        if (res > 0 && wrapper.poll_fd.revents != ZSOCK_POLLIN) {
            LOG_WRN("Poll reported events: %hd", wrapper.poll_fd.revents);
        }
        if (res == 0) {
            res = mqtt_ping(&wrapper.client);
            if (res) {
                LOG_WRN("Ping failed: %d", res);
                break;
            }
            continue;
        }
        res = do_input(&wrapper, NULL);
        if (res) {
            LOG_WRN("input failed: %d", res);
            break;
        }
    }

    res = mqtt_disconnect(&wrapper.client);
    LOG_INF("Disconnect with res: %d", res);
    return 0;
}

void run_mqtt_client(void) {
    int err;
    while (!(err = run_cycle())) {
        LOG_WRN("MQTT client resets due to transient errors");
        k_sleep(RETRY_PERIOD);
    }
    LOG_ERR("run_cycle() returned a fatal error: %d", err);
}
