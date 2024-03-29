#include <stdint.h>
#include <inttypes.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <models/gw.h>

#include "../main.h"
#include "mesh.h"

LOG_MODULE_REGISTER(mesh_mesh);

static void attention_on(struct bt_mesh_model *mod)
{
    LOG_INF("Attention on");
}

static void attention_off(struct bt_mesh_model *mod)
{
    LOG_INF("Attention off");
}

static struct hrtls_model_gw_handlers gw_handlers = {
    .push = loc_push_handler
};

static struct bt_mesh_health_srv health_srv = {
    .cb = &(const struct bt_mesh_health_srv_cb) {
        .attn_on = attention_on,
        .attn_off = attention_off,
    }
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

static struct bt_mesh_model vnd_models[] = {
    HRTLS_MODEL_GW(&gw_handlers)
};

static struct bt_mesh_model sig_models[] = {
    BT_MESH_MODEL_CFG_SRV,
    BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub)
};

static struct bt_mesh_elem elements[] = {
    BT_MESH_ELEM(0, sig_models, vnd_models)
};

static int prov_output_number(bt_mesh_output_action_t action, uint32_t number)
{
	LOG_INF("OOB Number: %u\n", number);
	return 0;
}

static void prov_complete(uint16_t net_idx, uint16_t addr)
{
    LOG_INF("Prov complete, net_idx: %" PRIu16 ", addr: %" PRIu16, net_idx, addr);
}

static void prov_reset(void)
{
    LOG_INF("Prov reset");
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
}

static const struct bt_mesh_prov prov = {
    .uuid = dev_uuid,
    .output_size = 4,
    .output_actions = BT_MESH_DISPLAY_NUMBER,
    .output_number = prov_output_number,
    .complete = prov_complete,
    .reset = prov_reset
};

static const struct bt_mesh_comp comp = {
    .cid = HRTLS_COMPANY_ID,
    .elem = elements,
    .elem_count = ARRAY_SIZE(elements)
};

static void self_provision(void) {
    static uint8_t net_key[16];
    static uint8_t dev_key[16];
    static uint8_t app_key[16];

    uint16_t addr = sys_get_le16(dev_uuid) & BIT_MASK(15);

    LOG_INF("Self-provisioning with address 0x%04" PRIx16, addr);
    int err = bt_mesh_provision(net_key, 0, 0, 0, addr, dev_key);
    if (err) {
        LOG_ERR("Provisioning failed, %d", err);
        hrtls_fail();
    }

    err = bt_mesh_app_key_add(0, 0, app_key);
    if (err) {
        LOG_ERR("App key add failed, %d", err);
    }

    vnd_models[0].keys[0] = 0;
}

void bt_ready(int err) {
    if (err) {
        LOG_ERR("Bluetooth init failed, %d", err);
        hrtls_fail();
    }

    LOG_INF("Bluetooth initialized");

    err = bt_mesh_init(&prov, &comp);
    if (err) {
        LOG_ERR("Mesh initialization failed, %d", err);
        hrtls_fail();
    }

    err = bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
    if (err) {
        LOG_ERR("Can't enable provisioning, %d", err);
    }

    self_provision();
}
