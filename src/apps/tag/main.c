#include <stdint.h>
#include <inttypes.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <models/tag.h>
#include <uwb/uwb.h>

#include "mesh/mesh.h"
#include "rtls/rtls.h"
#include "main.h"
#include "positioning.h"

LOG_MODULE_REGISTER(main);

uint8_t dev_uuid[16];

static struct bt_mesh_model *tag_model;

static void mesh_loc_push_work_handler(struct k_work *work);

static K_WORK_DEFINE(mesh_loc_push_work, mesh_loc_push_work_handler);
K_MSGQ_DEFINE(rtls_result_queue, sizeof(struct rtls_result), 3, 1);

static void mesh_loc_push_work_handler(struct k_work *work) {
    // TODO: temporarily hardcoded
    static const uint16_t gw_addr = 0x0100;

    struct rtls_result rtls_result;
    while (!k_msgq_get(&rtls_result_queue, &rtls_result, K_NO_WAIT)) {
        struct hrtls_model_gw_location loc = {
            .x = rtls_result.pos.x * 1000,
            .y = rtls_result.pos.y * 1000,
            .z = rtls_result.pos.z * 1000,
            .err = rtls_result.error * 1000
        };

        LOG_INF("Sending location to addr 0x%04" PRIx16, gw_addr);
        int res = hrtls_model_tag_loc_push(tag_model, gw_addr, &loc);
        LOG_INF("Send res: %d", res);
    }
}

static void send_location(const struct rtls_result *rtls_result) {
    if (k_msgq_put(&rtls_result_queue, rtls_result, K_NO_WAIT)) {
        LOG_WRN("Couldn't fit location into queue");
    }
    k_work_submit(&mesh_loc_push_work);
}

void hrtls_fail(void) {
    log_panic();
    k_fatal_halt(0);
}

void main(void) {
    LOG_INF("Initializing...");

    int err = uwb_module_initialize(UWB_TWR_MODE_SS);
    LOG_INF("uwb init res: %d", err);
    if (err) {
        hrtls_fail();
    }

    err = hwinfo_get_device_id(dev_uuid, sizeof(dev_uuid));
    if (err < 0) {
        LOG_ERR("Couldn't get device id, %d", err);
        for (uint8_t i = 0; i < ARRAY_SIZE(dev_uuid); i++) {
            dev_uuid[i] = 16 + i;
        }
    }

    err = mesh_initialize(&tag_model);
    if (err) {
        LOG_ERR("Bluetooth init failed, %d", err);
        hrtls_fail();
    }

    static const int64_t period_ms = 200;

    int64_t last_timestamp = k_uptime_get();
    while (true) {
        struct rtls_result rtls_result;
        err = perform_positioning(&rtls_result, 5);

        if (!err) {
            send_location(&rtls_result);
        }

        const int64_t next_timestamp = last_timestamp + period_ms;
        const int64_t now = k_uptime_get();
        if (next_timestamp > now) {
            last_timestamp = next_timestamp;
        }
        else {
            LOG_WRN("looks like the timestamp clock has slipped!");
            last_timestamp = now + period_ms;
        }
        int64_t ms_to_sleep = next_timestamp - now;
        LOG_INF("next positioning in %" PRId64 " ms", ms_to_sleep);
        k_sleep(K_MSEC(ms_to_sleep));
    }
}
