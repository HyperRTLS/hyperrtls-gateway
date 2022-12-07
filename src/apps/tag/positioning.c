#include <stddef.h>

#include <zephyr.h>
#include <zephyr/logging/log.h>

#include <models/tag.h>
#include <uwb/uwb.h>
#include <uwb/tag.h>

#include "rtls/rtls.h"

LOG_MODULE_REGISTER(positioning);

static const struct rtls_anchor anchors[] = {
    { // left corner by kitchen, ground
        .addr = 1,
        .pos = {
            .x = 0,
            .y = 0,
            .z = 0
        }
    },
    { // right corner by kitchen, on coffee table
        .addr = 2,
        .pos = {
            .x = 0,
            .y = 1.8,
            .z = 0.345
        }
    },
    { // left corner by bathroom, on ground
        .addr = 3,
        .pos = {
            .x = 1.8,
            .y = 1.8,
            .z = 0
        }
    },
    { // right corner by bathroom, on folding stool
        .addr = 4,
        .pos = {
            .x = 1.8,
            .y = 0,
            .z = 0.395
        }
    },
};

int perform_positioning(struct rtls_result *out_result) {
    static struct rtls_pos last_pos = { 0 };

    size_t anchor_indices[4];
    rtls_select_nearby_anchors(anchors, ARRAY_SIZE(anchors), last_pos, anchor_indices);

    struct rtls_measurement measurements[4];
    for (size_t i = 0; i < 4; i++) {
        const struct rtls_anchor *anchor = &anchors[anchor_indices[i]];
        int twr_res = uwb_tag_twr(CONFIG_HRTLS_PAN_ID, CONFIG_HRTLS_UWB_ADDR, anchor->addr, &measurements[i].distance);
        if (twr_res) {
            LOG_WRN("TWR fail with anchor %" PRIu16 ", res: %d", anchor->addr, twr_res);
            return -1;
        }
        measurements[i].anchor_pos = anchor->pos;
        k_sleep(K_MSEC(10));
    }

    int64_t start_time = k_uptime_get();
    struct rtls_result result;
    int fp_res = rtls_find_position(measurements, &result);
    if (fp_res) {
        LOG_WRN("Find position fail, res: %d", fp_res);
        return -2;
    }
    last_pos = result.pos;

    LOG_INF("Find position res %f/%f/%f/%f/%" PRId64,
            result.pos.x, result.pos.y, result.pos.z,
            result.error,
            k_uptime_get() - start_time);

    *out_result = result;
    return 0;
}
