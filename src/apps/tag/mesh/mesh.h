#pragma once

#include <stdint.h>

#include <zephyr/bluetooth/mesh.h>

#include <models/tag.h>

int mesh_initialize(struct bt_mesh_model **out_tag_model);
