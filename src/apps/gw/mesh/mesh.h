#pragma once

#include <stdint.h>

#include <models/gw.h>

void bt_ready(int err);
void loc_push_handler(uint16_t sender_addr, const struct hrtls_model_gw_location *location);
