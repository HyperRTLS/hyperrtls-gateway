#include <string.h>

#include <zephyr/bluetooth/mesh.h>
#include <zephyr/logging/log.h>

#include <models/gw.h>

LOG_MODULE_REGISTER(gw_impl);

static int handle_message_loc_push(struct bt_mesh_model *model,
                                    struct bt_mesh_msg_ctx *ctx,
                                    struct net_buf_simple *buf) {
    struct hrtls_model_gw_handlers *handlers = model->user_data;
    struct hrtls_model_gw_location location;

    if (buf->len != sizeof(location)) {
        LOG_ERR("Loc push incoming buffer has unexpected length: %" PRIu16, buf->len);
        return 0;
    }

    LOG_INF("Loc push message received");

    memcpy(&location, net_buf_simple_pull_mem(buf, sizeof(location)), sizeof(location));
    if (handlers->push) {
        handlers->push(ctx->addr, &location);
    }

    return 0;
}

const struct bt_mesh_model_op hrtls_model_gw_ops[] = {
    { HRTLS_MODEL_GW_LOC_PUSH_OPCODE, BT_MESH_LEN_EXACT(HRTLS_MODEL_GW_LOC_PUSH_LEN), handle_message_loc_push},
    BT_MESH_MODEL_OP_END
};
