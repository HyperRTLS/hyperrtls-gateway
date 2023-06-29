#pragma once

#include <stdint.h>

#include <uwb/uwb.h>

int uwb_tag_twr(uint16_t pan_id, uint16_t self_addr, uint16_t target_addr, float *out_distance_m);
