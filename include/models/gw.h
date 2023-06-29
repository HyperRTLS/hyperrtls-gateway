#pragma once
#include <stdint.h>

#include <zephyr/bluetooth/mesh.h>

#include <models/common.h>

struct hrtls_model_gw_location {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t err;
} __packed;

typedef void hrlts_model_gw_loc_push_handler_t(uint16_t sender_addr, const struct hrtls_model_gw_location *location);

struct hrtls_model_gw_handlers {
    hrlts_model_gw_loc_push_handler_t *push;
};

#define HRTLS_MODEL_GW_ID 0x0001

#define HRTLS_MODEL_GW_NODE_REG_OPCODE BT_MESH_MODEL_OP_3(0x01, HRTLS_COMPANY_ID)

#define HRTLS_MODEL_GW_LOC_PUSH_OPCODE BT_MESH_MODEL_OP_3(0x02, HRTLS_COMPANY_ID)
#define HRTLS_MODEL_GW_LOC_PUSH_LEN sizeof(struct hrtls_model_gw_location)

#define HRTLS_MODEL_GW(handlers) \
  BT_MESH_MODEL_VND_CB( \
    HRTLS_COMPANY_ID, \
    HRTLS_MODEL_GW_ID, \
    hrtls_model_gw_ops, \
    NULL, \
    handlers, \
    NULL)

extern const struct bt_mesh_model_op hrtls_model_gw_ops[];
