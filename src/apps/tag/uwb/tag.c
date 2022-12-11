#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr.h>
#include <zephyr/logging/log.h>

#include <dw1000/decadriver/deca_device_api.h>
#include <dw1000/decadriver/deca_regs.h>
#include <uwb/utils.h>
#include <uwb/tag.h>

LOG_MODULE_REGISTER(tag);

#define SS_POLL_RX_TS_OFFSET 10
#define SS_RESP_TX_TS_OFFSET 14

#define SS_POLL_TX_RESP_RX_DLY_UUS (140 + 800)
#define SS_RESP_RX_TIMEOUT_UUS 210

static void ss_poll_build(uint16_t pan_id,
                          uint16_t self_addr,
                          uint16_t target_addr,
                          uint8_t frame_seq_nb,
                          uint8_t *buf) {
    const uint8_t poll[] = {
        0x41, 0x88, // header
        frame_seq_nb,
        GET_BYTE(pan_id, 0), GET_BYTE(pan_id, 1),
        GET_BYTE(target_addr, 0), GET_BYTE(target_addr, 1),
        GET_BYTE(self_addr, 0), GET_BYTE(self_addr, 1),
        0xE0 // poll
    };

    static_assert(sizeof(poll) == SS_POLL_LEN);

    memcpy(buf, poll, SS_POLL_LEN);
}

static int ss_response_verify(uint16_t pan_id, uint16_t self_addr, uint16_t target_addr, const uint8_t *frame) {
    const uint8_t expected_header[] = {
        0x41, 0x88, //header
        0,
        GET_BYTE(pan_id, 0), GET_BYTE(pan_id, 1),
        GET_BYTE(self_addr, 0), GET_BYTE(self_addr, 1),
        GET_BYTE(target_addr, 0), GET_BYTE(target_addr, 1),
        0xE1 // response
    };

    const bool mask[] = {
        true, true,
        false,
        true, true,
        true, true,
        true, true,
        true
    };

    static_assert(ARRAY_SIZE(expected_header) == SS_RESP_LEN - 8);
    static_assert(ARRAY_SIZE(mask) == SS_RESP_LEN - 8);

    return masked_memcmp(expected_header, frame, mask, SS_RESP_LEN - 8);
}

static int uwb_tag_twr_ss(uint16_t pan_id, uint16_t self_addr, uint16_t target_addr, float *out_distance_m) {
    static uint8_t frame_seq_nb = 0;

    uint8_t poll_buf[SS_POLL_LEN];
    ss_poll_build(pan_id, self_addr, target_addr, frame_seq_nb++, poll_buf);

    // reset RX
    clear_status(SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_RX_TO);
    dwt_rxreset();

    dwt_setrxaftertxdelay(SS_POLL_TX_RESP_RX_DLY_UUS);
    dwt_setrxtimeout(SS_RESP_RX_TIMEOUT_UUS);

    if (send_frame_ff(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED, poll_buf, ARRAY_SIZE(poll_buf))) {
        return -2;
    }

    uint32_t status_reg = wait_for_status(SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_RX_TO);
    if (!(status_reg & SYS_STATUS_RXFCG)) {
        return -3;
    }

    uint8_t response_buf[SS_RESP_LEN];
    if (read_frame(response_buf, ARRAY_SIZE(response_buf))) {
        return -4;
    }

    if (ss_response_verify(pan_id, self_addr, target_addr, response_buf)) {
        LOG_HEXDUMP_INF(response_buf, SS_RESP_LEN, "XD");
        return -5;
    }

    uint32_t poll_tx_ts = dwt_readtxtimestamplo32();
    uint32_t poll_rx_ts = (uint32_t)buf_le_to_u64(response_buf + SS_POLL_RX_TS_OFFSET, 4);
    uint32_t resp_tx_ts = (uint32_t)buf_le_to_u64(response_buf + SS_RESP_TX_TS_OFFSET, 4);
    uint32_t resp_rx_ts = dwt_readrxtimestamplo32();
    float clock_offset_ratio = dwt_readcarrierintegrator() * (FREQ_OFFSET_MULTIPLIER * HERTZ_TO_PPM_MULTIPLIER_CHAN_2 / 1e6);

    uint32_t tx_to_rx_tag = resp_rx_ts - poll_tx_ts;
    uint32_t rx_to_tx_anchor = resp_tx_ts - poll_rx_ts;

    float tof = ((tx_to_rx_tag - rx_to_tx_anchor * (1 - clock_offset_ratio)) / 2e0) * DWT_TIME_UNITS;
    *out_distance_m = tof * SPEED_OF_LIGHT_M_S;
    return 0;
}

static int uwb_tag_twr_ds(uint16_t pan_id, uint16_t self_addr, uint16_t target_addr, float *out_distance_m) {
    assert(false);
    return -1;
}

int uwb_tag_twr(uint16_t pan_id, uint16_t self_addr, uint16_t target_addr, float *out_distance_m) {
    int (*impls[])(uint16_t pan_id, uint16_t self_addr, uint16_t target_addr, float *out_distance_m) = {
        [UWB_TWR_MODE_SS] = uwb_tag_twr_ss,
        [UWB_TWR_MODE_DS] = uwb_tag_twr_ds
    };

    if (uwb_current_mode < 0 || uwb_current_mode > ARRAY_SIZE(impls)) {
        return -1;
    }

    return impls[uwb_current_mode](pan_id, self_addr, target_addr, out_distance_m);
}
