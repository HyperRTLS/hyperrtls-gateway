#include <assert.h>
#include <stdatomic.h>
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
#define RETRY_PERIOD K_SECONDS(15)
#define BUF_SIZE 2048
#define TIMEOUT_MS 5000
#define BATCH_MAX 32

struct client_wrapper;
typedef void evt_cb_handler_t(const struct mqtt_evt *evt);

struct client_wrapper {
    const struct gw_mqtt_client_config *config;
    struct mqtt_client client;
    struct mqtt_utf8 username;
    struct mqtt_utf8 password;
    struct sockaddr_in addr;
    uint8_t rx_buffer[BUF_SIZE];
    uint8_t tx_buffer[BUF_SIZE];
    atomic_uint_fast16_t next_mid;
    struct zsock_pollfd poll_fd;

    evt_cb_handler_t *overrided_evt_cb_handler;
    // TODO: reverse evt_err logic?
    int evt_err;

    bool running;
};

// FIXME: this being global isn't really elegant
static struct client_wrapper wrapper;
static K_MUTEX_DEFINE(wrapper_ext_access_mutex);

static uint16_t get_next_mid(void) {
    uint16_t res;
    do {
        res = (uint16_t) atomic_fetch_add(&wrapper.next_mid, 1);
    }
    while (res == 0); // Message ID == 0 is disallowed by the spec
    return res;
}

static inline struct mqtt_utf8 make_mqtt_utf8(const char *str) {
    assert(str);
    return (struct mqtt_utf8) {
        .utf8 = str,
        .size = strlen(str)
    };
}

static void evt_cb_handler_connack(const struct mqtt_evt *evt) {
    if (evt->type != MQTT_EVT_CONNACK) {
        LOG_WRN("Unexpected packet type");
        wrapper.evt_err = -1;
        return;
    }
    if (evt->result) {
        LOG_WRN("Connect failed: %d", evt->result);
        wrapper.evt_err = -1;
        return;
    }
    LOG_INF("Server return code: %d", (int) evt->param.connack.return_code);
}

static void evt_cb_handler_suback(const struct mqtt_evt *evt) {
    if (evt->type != MQTT_EVT_SUBACK) {
        LOG_WRN("Unexpected packet type");
        wrapper.evt_err = -1;
        return;
    }
    LOG_INF("Suback received");
}

static void evt_cb_handler_default(const struct mqtt_evt *evt) {
    // TODO: more sophisticated event handling
    LOG_INF("Received evt %d", (int) evt->type);

    if (evt->type != MQTT_EVT_PUBLISH) {
        return;
    }

    uint32_t len = evt->param.publish.message.payload.len;
    LOG_INF("Received %" PRIu32 " bytes of payload", len);

    static uint8_t incoming_publish_buf[BUF_SIZE];
    if (len > sizeof(incoming_publish_buf)) {
        LOG_ERR("Received a publish packet with payload larger than BUF_SIZE");
        wrapper.evt_err = -1;
        return;
    }

    int res = mqtt_readall_publish_payload(&wrapper.client, incoming_publish_buf, len);
    if (res) {
        LOG_ERR("Publish readall failed with result %d", res);
        wrapper.evt_err = res;
        return;
    }

    wrapper.config->pub_handler(incoming_publish_buf, len);
}

static void mqtt_evt_cb(struct mqtt_client *const client,
                        const struct mqtt_evt *evt) {
    assert(!wrapper.evt_err);
    evt_cb_handler_t *handler = wrapper.overrided_evt_cb_handler ? wrapper.overrided_evt_cb_handler : evt_cb_handler_default;
    handler(evt);
}

static void init_mqtt_client(const struct gw_mqtt_client_config *config) {
    assert(wrapper);

    wrapper.config = config;

    wrapper.username = make_mqtt_utf8(config->username);
    wrapper.password = make_mqtt_utf8(config->password);
    wrapper.overrided_evt_cb_handler = NULL;
    atomic_store(&wrapper.next_mid, 0);

    mqtt_client_init(&wrapper.client);

    wrapper.client.user_name = &wrapper.username;
    wrapper.client.password = &wrapper.password;
    wrapper.client.client_id = wrapper.username;

    wrapper.client.evt_cb = mqtt_evt_cb;
    wrapper.client.broker = &wrapper.addr;
    wrapper.client.protocol_version = MQTT_VERSION_3_1_1;
    wrapper.client.transport.type = MQTT_TRANSPORT_NON_SECURE;
    wrapper.client.clean_session = true;
    wrapper.client.rx_buf = wrapper.rx_buffer;
    wrapper.client.rx_buf_size = sizeof(wrapper.rx_buffer);
    wrapper.client.tx_buf = wrapper.tx_buffer;
    wrapper.client.tx_buf_size = sizeof(wrapper.tx_buffer);
}

static int configure_mqtt_addr(void) {
    int res = resolve_addr(wrapper.config->server_addr, wrapper.config->server_port, &wrapper.addr);
    if (!res) {
        wrapper.client.broker = &wrapper.addr;
    }
    return res;
}

static void init_poll_fd(void) {
    wrapper.poll_fd.fd = wrapper.client.transport.tcp.sock;
    wrapper.poll_fd.events = ZSOCK_POLLIN;
}

static int do_poll(int wait_ms) {
    return zsock_poll(&wrapper.poll_fd, 1, wait_ms);
}

static int do_await(int wait_ms) {
    int res = do_poll(wait_ms);
    if (res == 0) {
        LOG_WRN("Poll timed out");
        return -1;
    }
    if (res < 0 || wrapper.poll_fd.revents != ZSOCK_POLLIN) {
        LOG_WRN("Poll failed with res %d, revents: %d", res, wrapper.poll_fd.revents);
        return res;
    }
    return 0;
}

static int do_input(evt_cb_handler_t *evt_cb_handler) {
    wrapper.overrided_evt_cb_handler = evt_cb_handler;
    wrapper.evt_err = 0;

    LOG_INF("pre input");
    int res = mqtt_input(&wrapper.client);
    LOG_INF("after input");
    if (res || wrapper.evt_err) {
        LOG_WRN("do_input failed with res: %d, evt_err: %d", res, wrapper.evt_err);
    }
    wrapper.overrided_evt_cb_handler = NULL;
    return res ? res : wrapper.evt_err;
}

static int do_connect(void) {
    int res = configure_mqtt_addr();
    if (res) {
        LOG_WRN("DNS resolution failed, assuming a transient error");
        return -1;
    }

    res = mqtt_connect(&wrapper.client);
    if (res) {
        LOG_WRN("Connect failed, assuming a transient error");
        return -1;
    }
    LOG_INF("MQTT client socket connected");
    init_poll_fd();

    res = do_await(TIMEOUT_MS);
    if (res) {
        goto abort;
    }

    res = do_input(evt_cb_handler_connack);
    if (res) {
        goto abort;
    }

    return 0;

abort:;
    int abort_res = mqtt_abort(&wrapper.client);
    if (abort_res) {
        LOG_WRN("abort failed with res: %d", abort_res);
    }
    return res;
}

static int do_subscribe(void) {
    struct mqtt_topic *topic_list = malloc(wrapper.config->topics_len * sizeof(*topic_list));
    if (!topic_list) {
        return FATAL_ERROR;
    }
    for (size_t i = 0; i < wrapper.config->topics_len; i++) {
        topic_list[i] = (struct mqtt_topic) {
            .topic = make_mqtt_utf8(wrapper.config->topics[i]),
            .qos = 0
        };
    }
    const struct mqtt_subscription_list subs_list = {
        .list = topic_list,
        .list_count = wrapper.config->topics_len,
        .message_id = get_next_mid()
    };

    LOG_INF("Attempting to %zu topics", subs_list.list_count);
    int res = mqtt_subscribe(&wrapper.client, &subs_list);

    free(topic_list);

    if (res) {
        LOG_ERR("mqtt_subscribe() failed");
        return res;
    }

    res = do_await(TIMEOUT_MS);
    if (res) {
        return -1;
    }

    return do_input(evt_cb_handler_suback);
}

static int run_cycle(const struct gw_mqtt_client_config *config) {
    init_mqtt_client(config);

    int res = do_connect();
    if (res) {
        return res;
    }

    res = do_subscribe();
    if (res) {
        return res;
    }

    k_mutex_lock(&wrapper_ext_access_mutex, K_FOREVER);
    wrapper.running = true;
    k_mutex_unlock(&wrapper_ext_access_mutex);

    while (true) {
        res = do_poll(mqtt_keepalive_time_left(&wrapper.client));
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
        res = do_input(NULL);
        if (res) {
            LOG_WRN("input failed: %d", res);
            break;
        }
    }

    k_mutex_lock(&wrapper_ext_access_mutex, K_FOREVER);
    wrapper.running = false;
    res = mqtt_disconnect(&wrapper.client);
    k_mutex_unlock(&wrapper_ext_access_mutex);

    LOG_INF("Disconnect with res: %d", res);
    return 0;
}

static int gw_mqtt_client_try_publishing_unlocked(const char *topic, const uint8_t *message, size_t len) {
    if (!wrapper.running) {
        return -EAGAIN;
    }

    struct mqtt_publish_param param = {
        .message = (struct mqtt_publish_message) {
            .topic = (struct mqtt_topic) {
                .topic = make_mqtt_utf8(topic),
                .qos = 0
            },
            .payload = (struct mqtt_binstr) {
                .data = (uint8_t *) message,
                .len = len
            }
        },
        .message_id = get_next_mid(),
        .dup_flag = false,
        .retain_flag = false
    };

    return mqtt_publish(&wrapper.client, &param);
}

int gw_mqtt_client_try_publishing(const char *topic, const uint8_t *message, size_t len) {
    int res;
    k_mutex_lock(&wrapper_ext_access_mutex, K_FOREVER);
    res = gw_mqtt_client_try_publishing_unlocked(topic, message, len);
    k_mutex_unlock(&wrapper_ext_access_mutex);
    return res;
}

void gw_mqtt_client_run(const struct gw_mqtt_client_config *config) {
    assert(config);
    assert(config->server_addr);
    assert(config->username);
    assert(config->password);
    assert(config->topics_len > 0);
    assert(config->pub_handler);
    int err;
    while ((err = run_cycle(config)) != FATAL_ERROR) {
        LOG_WRN("MQTT client resets due to transient errors");
        k_sleep(RETRY_PERIOD);
    }
    LOG_ERR("run_cycle() returned a fatal error: %d", err);
}
