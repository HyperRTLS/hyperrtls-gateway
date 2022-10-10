#include <stdint.h>
#include <inttypes.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <models/gw.h>

#include "../main.h"
#include "mesh.h"

LOG_MODULE_REGISTER(mesh_handlers);

void loc_push_handler(uint16_t sender_addr, const struct hrtls_model_gw_location *location) {
    LOG_INF("addr %" PRIu16 "sent location %f/%f/%f", sender_addr, location->x, location->y, location->z);
}
