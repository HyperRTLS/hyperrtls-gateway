#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <dw1000/decadriver/deca_device_api.h>
#include <dw1000/decadriver/deca_regs.h>

#include <uwb/utils.h>

int masked_memcmp(const uint8_t *left,
                         const uint8_t *right,
                         const bool *mask,
                         size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (!mask[i]) {
            continue;
        }
        if (left[i] != right[i]) {
            return -1;
        }
    }
    return 0;
}

uint64_t buf_le_to_u64(const uint8_t *buf, size_t len) {
    assert(len <= 8);

    uint64_t res = 0;
    for (size_t i = 0; i < len; i++) {
        res |= SET_BYTE(buf[i], i);
    }
    return res;
}

uint32_t wait_for_status(uint32_t mask) {
    uint32_t status_reg;
    do {
        status_reg = dwt_read32bitreg(SYS_STATUS_ID);
    }
    while (!(status_reg & mask));
    return status_reg;
}

void clear_status(uint32_t mask) {
    dwt_write32bitreg(SYS_STATUS_ID, mask);
}

uint16_t read_frame_len(void) {
    uint16_t frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023;
    if (frame_len < 2) {
        return 0;
    }
    // subtract trailing CRC
    return frame_len - 2;
}

int read_frame(uint8_t *buf, uint16_t expected_frame_len) {
    uint16_t frame_len = read_frame_len();
    if (frame_len != expected_frame_len) {
        return -1;
    }

    dwt_readrxdata(buf, frame_len, 0);
    return 0;
}

static int send_frame(uint8_t mode, const uint8_t *frame, uint16_t frame_len) {
    dwt_writetxdata(frame_len + 2, (uint8_t *)frame, 0);
    dwt_writetxfctrl(frame_len + 2, 0, 1);

    if (dwt_starttx(mode)) {
        return -1;
    }

    return 0;
}

int send_frame_ack(uint8_t mode, const uint8_t *frame, uint16_t frame_len) {
    if (send_frame(mode, frame, frame_len)) {
        return -1;
    }

    wait_for_status(SYS_STATUS_TXFRS);
    clear_status(SYS_STATUS_TXFRS);
    return 0;
}

int send_frame_ff(uint8_t mode, const uint8_t *frame, uint16_t frame_len) {
    return send_frame(mode, frame, frame_len);
}
