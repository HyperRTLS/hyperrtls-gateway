#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr.h>
#include <zephyr/logging/log.h>

#include <dw1000/decadriver/deca_device_api.h>
#include <dw1000/decadriver/deca_regs.h>
#include <uwb/utils.h>
#include <uwb/anchor.h>

#define TARGET_ADDR_OFFSET 5

LOG_MODULE_REGISTER(anchor);

// measured experimentally
#define SS_POLL_RX_RESP_TX_DLY_UUS (330 + 800)
// max rx timeout possible (16bit), about 70ms
#define SS_RX_TIMEOUT_UUS 65535

static int ss_poll_verify(uint16_t pan_id,
                          uint16_t self_addr,
                          const uint8_t *frame) {
    // TODO: consider X-Macroing the following
    const uint8_t expected[] = {
        0x41, 0x88, // header
        0,
        GET_BYTE(pan_id, 0), GET_BYTE(pan_id, 1),
        0, 0,
        GET_BYTE(self_addr, 0), GET_BYTE(self_addr, 1),
        0xE0 // poll
    };

    const bool mask[] = {
        true, true,
        false,
        true, true,
        false, false,
        true, true,
        true
    };

    static_assert(ARRAY_SIZE(expected) == SS_POLL_LEN);
    static_assert(ARRAY_SIZE(mask) == SS_POLL_LEN);

    return masked_memcmp(expected, frame,
                         mask, SS_POLL_LEN);
}

static void ss_response_build(uint16_t pan_id,
                              uint16_t self_addr,
                              uint16_t target_addr,
                              uint64_t poll_rx_ts,
                              uint64_t resp_tx_ts,
                              uint8_t frame_seq_nb,
                              uint8_t *frame) {
    const uint8_t response[] = {
        0x41, 0x88, // header
        frame_seq_nb,
        GET_BYTE(pan_id, 0), GET_BYTE(pan_id, 1),
        GET_BYTE(self_addr, 0), GET_BYTE(self_addr, 1),
        GET_BYTE(target_addr, 0), GET_BYTE(target_addr, 1),
        0xE1, // response
        GET_BYTE(poll_rx_ts, 0), GET_BYTE(poll_rx_ts, 1),
        GET_BYTE(poll_rx_ts, 2), GET_BYTE(poll_rx_ts, 3),
        GET_BYTE(resp_tx_ts, 0), GET_BYTE(resp_tx_ts, 1),
        GET_BYTE(resp_tx_ts, 2), GET_BYTE(resp_tx_ts, 3)
    };

    static_assert(sizeof(response) == SS_RESP_LEN);

    memcpy(frame, response, SS_RESP_LEN);
}

static int uwb_anchor_twr_ss(uint16_t pan_id, uint16_t self_addr) {
    static uint8_t frame_seq_nb = 0;

    dwt_setrxtimeout(SS_RX_TIMEOUT_UUS);
    dwt_rxenable(DWT_START_RX_IMMEDIATE);

    uint32_t status_reg = wait_for_status(SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_RX_TO);
    if (!(status_reg & SYS_STATUS_RXFCG)) {
        return -2;
    }

    uint8_t poll_buf[SS_POLL_LEN];
    if (read_frame(poll_buf, ARRAY_SIZE(poll_buf))) {
        return -3;
    }
    if (ss_poll_verify(pan_id, self_addr, poll_buf)) {
        return -4;
    }

    uint16_t target_addr = (uint16_t)buf_le_to_u64(poll_buf + TARGET_ADDR_OFFSET, 2);

    uint8_t poll_rx_ts_buf[5];
    dwt_readrxtimestamp(poll_rx_ts_buf);

    uint64_t poll_rx_ts = buf_le_to_u64(poll_rx_ts_buf, ARRAY_SIZE(poll_rx_ts_buf));
    uint32_t resp_tx_dt = TS_TO_DT(poll_rx_ts + UUS_TO_DWT_TIME * SS_POLL_RX_RESP_TX_DLY_UUS);
    uint64_t resp_tx_ts = DT_TO_TS(resp_tx_dt) + TX_ANT_DLY;

    dwt_setdelayedtrxtime(resp_tx_dt);

    uint8_t response_buf[SS_RESP_LEN];
    ss_response_build(pan_id, self_addr, target_addr, poll_rx_ts, resp_tx_ts, frame_seq_nb, response_buf);
    if (send_frame_ack(DWT_START_TX_DELAYED, response_buf, SS_RESP_LEN)) {
        return -5;
    }
    frame_seq_nb++;
    return 0;
}

static int uwb_anchor_twr_ds(uint16_t pan_id, uint16_t self_addr) {
    assert(false);
    return -1;
}

int uwb_anchor_twr(uint16_t pan_id, uint16_t self_addr) {
    int (*impls[])(uint16_t pan_id, uint16_t self_addr) = {
        [UWB_TWR_MODE_SS] = uwb_anchor_twr_ss,
        [UWB_TWR_MODE_DS] = uwb_anchor_twr_ds
    };

    if (uwb_current_mode < 0 || uwb_current_mode > ARRAY_SIZE(impls)) {
        return -1;
    }

    int res = impls[uwb_current_mode](pan_id, self_addr);

    // reset RX
    clear_status(SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_RX_TO);
    dwt_rxreset();

    return res;
}
