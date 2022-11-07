#include <stdint.h>
#include <inttypes.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include "mesh/mesh.h"
#include "main.h"
#include "mqtt.h"

LOG_MODULE_REGISTER(main);

static void fake_loc_push_dwork_handler(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(fake_loc_push_dwork, fake_loc_push_dwork_handler);

uint8_t dev_uuid[16];

static void pub_handler(const uint8_t *buffer, size_t len) {
    LOG_HEXDUMP_INF(buffer, len, "pub_handler");
}

static void fake_loc_push_dwork_handler(struct k_work *work) {
    static const char *msg = "test-msg";
    int res = gw_mqtt_client_try_publishing("tag/some-tag-id/loc", msg, strlen(msg));
    if (res) {
        LOG_WRN("Loc push failed with res: %d", res);
    }
    else {
        LOG_INF("Loc push successful");
    }

    k_work_schedule(&fake_loc_push_dwork, K_SECONDS(5));
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

    const char *const topics[] = {
        "gw/probably-unique/config"
    };

    const struct gw_mqtt_client_config client_config = {
        .server_addr = "test.mosquitto.org",
        .server_port = 1883,
        .username = "probably-unique",
        .password = "pass",
        .topics = topics,
        .topics_len = ARRAY_SIZE(topics),
        .pub_handler = pub_handler
    };

    k_work_schedule(&fake_loc_push_dwork, K_SECONDS(5));
    gw_mqtt_client_run(&client_config);
    LOG_ERR("MQTT client unexpectedly returned");
    hrtls_fail();
}
