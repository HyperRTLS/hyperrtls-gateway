#include <string.h>

#include <zephyr/bluetooth/mesh.h>
#include <zephyr/logging/log.h>

#include <models/tag.h>

LOG_MODULE_REGISTER(tag_impl);

const struct bt_mesh_model_op hrtls_model_tag_ops[] = {
    BT_MESH_MODEL_OP_END
};

int hrtls_model_tag_loc_push(struct bt_mesh_model *tag_model, uint16_t addr, struct hrtls_model_gw_location *location) {
    struct bt_mesh_msg_ctx ctx = {
        .addr = addr,
        .app_idx = tag_model->keys[0],
        .send_ttl = BT_MESH_TTL_DEFAULT,
        .send_rel = true
    };

    BT_MESH_MODEL_BUF_DEFINE(buf, HRTLS_MODEL_GW_LOC_PUSH_OPCODE, HRTLS_MODEL_GW_LOC_PUSH_LEN);
    bt_mesh_model_msg_init(&buf, HRTLS_MODEL_GW_LOC_PUSH_OPCODE);
    net_buf_simple_add_mem(&buf, location, sizeof(*location));

    return bt_mesh_model_send(tag_model, &ctx, &buf, NULL, NULL);
}
