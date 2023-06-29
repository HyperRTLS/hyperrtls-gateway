#pragma once

#include <stddef.h>
#include <stdint.h>

struct rtls_pos {
    float x;
    float y;
    float z;
};

struct rtls_anchor {
    uint16_t addr;
    struct rtls_pos pos;
};

struct rtls_measurement {
    struct rtls_pos anchor_pos;
    float distance;
};

struct rtls_result {
    struct rtls_pos pos;
    float error;
};

int rtls_select_nearby_anchors(const struct rtls_anchor anchors[],
                               size_t n,
                               struct rtls_pos last_pos,
                               size_t out_anchor_indices[4]);

int rtls_find_position(const struct rtls_measurement measurements[4],
                       struct rtls_result *out_result);
