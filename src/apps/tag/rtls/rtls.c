#include <assert.h>
#include <errno.h>
#include <math.h>
#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <zsl/zsl.h>
#include <zsl/matrices.h>

#include "rtls.h"

struct cmp_args {
    const struct rtls_anchor *anchors;
    struct rtls_pos last_pos;
};

static inline float powf2(float x) {
    return x * x;
}

static inline float vec3_len_pow2(float x, float y, float z) {
    return powf2(x) + powf2(y) + powf2(z);
}

static inline float pos_pow2(const struct rtls_pos *pos) {
    return vec3_len_pow2(pos->x, pos->y, pos->z);
}

static int anchor_indices_cmp(void *arg, const void *a, const void *b) {
    struct cmp_args *args = arg;
    const struct rtls_pos *last_pos = &args->last_pos;
    const struct rtls_pos *a_pos = &args->anchors[*(const size_t *)a].pos;
    const struct rtls_pos *b_pos = &args->anchors[*(const size_t *)b].pos;

    float dist_a_pow2 = vec3_len_pow2(a_pos->x - last_pos->x,
                                      a_pos->y - last_pos->y,
                                      a_pos->z - last_pos->z);
    float dist_b_pow2 = vec3_len_pow2(b_pos->x - last_pos->x,
                                      b_pos->y - last_pos->y,
                                      b_pos->z - last_pos->z);

    float dist_pow2_diff = dist_a_pow2 - dist_b_pow2;
    if (dist_pow2_diff < 0) {
        return -1;
    }
    else if (dist_pow2_diff > 0) {
        return 1;
    }

    return 0;
}

int rtls_select_nearby_anchors(const struct rtls_anchor anchors[],
                               size_t n,
                               struct rtls_pos last_pos,
                               size_t out_anchor_indices[4]) {
    assert(anchors);
    assert(out_anchor_indices);
    assert(n >= 4);

    size_t *indices = malloc(n * sizeof(*indices));
    if (!indices) {
        return -ENOMEM;
    }

    for (size_t i = 0; i < n; i++) {
        indices[i] = i;
    }

    struct cmp_args args = {
        .anchors = anchors,
        .last_pos = last_pos
    };

    qsort_r(indices, n, sizeof(*indices), &args, anchor_indices_cmp);
    memcpy(out_anchor_indices, indices, 4 * sizeof(*indices));

    return 0;
}

int rtls_find_position(const struct rtls_measurement measurements[4],
                       struct rtls_result *out_result) {
    assert(measurements);
    assert(out_result);

    ZSL_MATRIX_DEF(A, 4, 4);
    for (size_t row = 0; row < 4; row++) {
        zsl_mtx_set(&A, row, 0, 1);
        zsl_mtx_set(&A, row, 1, -2 * measurements[row].anchor_pos.x);
        zsl_mtx_set(&A, row, 2, -2 * measurements[row].anchor_pos.y);
        zsl_mtx_set(&A, row, 3, -2 * measurements[row].anchor_pos.z);
    }

    zsl_real_t A_det;
    zsl_mtx_deter(&A, &A_det);
    if (A_det == 0) {
        // det == 0 <=> anchors are coplanar
        return -EINVAL;
    }

    ZSL_MATRIX_DEF(A_inv, 4, 4);
    zsl_mtx_inv(&A, &A_inv);

    ZSL_MATRIX_DEF(b, 4, 1);
    for (size_t row = 0; row < 4; row++) {
        zsl_mtx_set(&b, row, 0, powf2(measurements[row].distance) - pos_pow2(&measurements[row].anchor_pos));
    }

    ZSL_MATRIX_DEF(res, 4, 1);
    zsl_mtx_mult(&A_inv, &b, &res);

    float w, x, y, z;
    zsl_mtx_get(&res, 0, 0, &w);
    zsl_mtx_get(&res, 1, 0, &x);
    zsl_mtx_get(&res, 2, 0, &y);
    zsl_mtx_get(&res, 3, 0, &z);

    out_result->error = fabsf(w - vec3_len_pow2(x, y, z));
    out_result->pos.x = x;
    out_result->pos.y = y;
    out_result->pos.z = z;

    return 0;
}
