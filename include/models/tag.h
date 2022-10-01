#pragma once
#include <zephyr/bluetooth/mesh.h>

#include <models/common.h>
#include <models/gw.h>

#define HRTLS_MODEL_TAG_ID 0x0003
#define HRTLS_MODEL_TAG_CONF_UPDATE_OPCODE BT_MESH_MODEL_OP_3(0x05, HRTLS_COMPANY_ID)

#define HRTLS_MODEL_TAG \
  BT_MESH_MODEL_VND_CB( \
    HRTLS_COMPANY_ID, \
    HRTLS_MODEL_TAG_ID, \
    hrtls_model_tag_ops, \
    NULL, \
    NULL, \
    NULL)

int hrtls_model_tag_loc_push(struct bt_mesh_model *tag_model, uint16_t addr, struct hrtls_model_gw_location *location);

extern const struct bt_mesh_model_op hrtls_model_tag_ops[];
