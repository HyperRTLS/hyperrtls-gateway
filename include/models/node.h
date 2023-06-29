#pragma once
#include <zephyr/bluetooth/mesh.h>

#include <models/common.h>

#define HRTLS_MODEL_NODE_ID 0x0002
#define HRTLS_MODEL_NODE_REG_ACK_OPCODE BT_MESH_MODEL_OP_3(0x03, HRTLS_COMPANY_ID)
#define HRTLS_MODEL_NODE_KEEPALIVE_OPCODE BT_MESH_MODEL_OP_3(0x04, HRTLS_COMPANY_ID)

#define HRTLS_MODEL_NODE \
  BT_MESH_MODEL_VND_CB( \
    HRTLS_COMPANY_ID, \
    HRTLS_MODEL_NODE_ID, \
    hrtls_model_node_ops, \
    NULL, \
    NULL, \
    NULL)

enum hrtls_node_kind {
    HRTLS_NODE_KIND_ANCHOR,
    HRTLS_NODE_KIND_TAG
};

extern const struct bt_mesh_model_op hrtls_model_node_ops[];
