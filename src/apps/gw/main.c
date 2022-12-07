#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <models/gw.h>

#include "mesh/mesh.h"
#include "main.h"
#include "mqtt.h"

LOG_MODULE_REGISTER(main);

#define POSITION_FORMAT "{\"position\":[%f,%f,%f]}"

uint8_t dev_uuid[16];

static void mqtt_loc_push_work_handler(struct k_work *work);
static K_WORK_DEFINE(mqtt_loc_push_work, mqtt_loc_push_work_handler);
K_MSGQ_DEFINE(locs_queue, sizeof(struct hrtls_model_gw_location), 3, 1);

static void mqtt_loc_push_work_handler(struct k_work *work) {
    struct hrtls_model_gw_location loc;
    while (!k_msgq_get(&locs_queue, &loc, K_NO_WAIT)) {
        static uint8_t msg_buf[256];
        int res = snprintf(msg_buf, sizeof(msg_buf), POSITION_FORMAT, loc.x, loc.y, loc.z);
        assert(res > 0);
        res = gw_mqtt_client_try_publishing("gateways/gateway_1/tags/tag_1/position", msg_buf, res);
        if (res) {
            LOG_WRN("Loc push failed with res: %d", res);
        }
        else {
            LOG_INF("Loc push successful");
        }
    }
}

static void pub_handler(const uint8_t *buffer, size_t len) {
    LOG_HEXDUMP_INF(buffer, len, "pub_handler");
}

void loc_push_handler(uint16_t sender_addr, const struct hrtls_model_gw_location *location) {
    LOG_INF("addr %" PRIu16 "sent location %f/%f/%f", sender_addr, location->x, location->y, location->z);
    if (k_msgq_put(&locs_queue, location, K_NO_WAIT)) {
        LOG_WRN("Couldn't fit location into queue");
    }
    k_work_submit(&mqtt_loc_push_work);
}

void hrtls_fail(void) {
    log_panic();
    k_fatal_halt(0);
}

void main(void) {
    LOG_INF("Initializing...");

    // FIXME: Why this fails for qemu_x86 target? lol
    int err;
    if ((err = hwinfo_get_device_id(dev_uuid, sizeof(dev_uuid))) < 0) {
        LOG_ERR("Couldn't get device id, %d", err);
        for (uint8_t i = 0; i < ARRAY_SIZE(dev_uuid); i++) {
            dev_uuid[i] = i;
        }
    }

    if ((err = bt_enable(bt_ready))) {
        LOG_ERR("Bluetooth init failed, %d", err);
        hrtls_fail();
    }

    // TODO: debug why this delay is needed when using E1000 driver on qemu_x86
    // TI Stellaris on qemu_cortex_m3 does not have this problem, but is
    // incompatible with BT unix socket proxy
    k_sleep(K_SECONDS(5));

    const char *const topics[] = {};

    const struct gw_mqtt_client_config client_config = {
        .server_addr = "192.0.2.2",
        .server_port = 1883,
        .username = "gateway_1",
        .password = "pwd",
        .topics = topics,
        .topics_len = ARRAY_SIZE(topics),
        .pub_handler = pub_handler
    };

    gw_mqtt_client_run(&client_config);
    LOG_ERR("MQTT client unexpectedly returned");
    hrtls_fail();
}
