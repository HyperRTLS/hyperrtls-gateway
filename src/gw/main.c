#include <stdint.h>

#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/mesh.h>

#include <models/common.h>
#include <models/gw.h>

LOG_MODULE_REGISTER(main);

static void loc_push_handler(uint16_t sender_addr, const struct hrtls_model_gw_location *location) {
    LOG_INF("addr %" PRIu16 "sent location %f/%f/%f", sender_addr, location->x, location->y, location->z);
}

static struct hrtls_model_gw_handlers gw_handlers = {
    .push = loc_push_handler
};

static struct bt_mesh_model vnd_models[] = {
    HRTLS_MODEL_GW(&gw_handlers)
};

static struct bt_mesh_model sig_models[] = {
    BT_MESH_MODEL_CFG_SRV,
    BT_MESH_MODEL_HEALTH_SRV(NULL, NULL)
};

static struct bt_mesh_elem elements[] = {
    BT_MESH_ELEM(0, sig_models, vnd_models)
};

static const struct bt_mesh_comp composition = {
    .cid = HRTLS_COMPANY_ID,
    .elem = elements,
    .elem_count = ARRAY_SIZE(elements)
};

void main(void) { }
