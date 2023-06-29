#pragma once

#define GET_BYTE(var, i) (((var) >> ((i) * 8)) % 256)
#define SET_BYTE(var, i) (((uint64_t)(var)) << ((i) * 8))

// converts 40bit timestamp (stored in 64bit variable) to 32bit TRX time
// with last bit cleared (delayed TRX can be scheduled every 512 cycles)
#define TS_TO_DT(timestamp) (((uint32_t)((timestamp) >> 8)) & ~1)
#define DT_TO_TS(delayed_time) (((uint64_t)(delayed_time)) << 8)

#define SPEED_OF_LIGHT_M_S 299702547
#define UUS_TO_DWT_TIME 65536
#define TX_ANT_DLY 16436
#define RX_ANT_DLY 16436

#define SS_POLL_LEN 10
#define SS_RESP_LEN 18

int masked_memcmp(const uint8_t *left,
                  const uint8_t *right,
                  const bool *mask,
                  size_t len);
uint64_t buf_le_to_u64(const uint8_t *buf, size_t len);
uint32_t wait_for_status(uint32_t mask);
void clear_status(uint32_t mask);
uint16_t read_frame_len(void);
int read_frame(uint8_t *buf, uint16_t expected_frame_len);
int send_frame_ack(uint8_t mode, const uint8_t *frame, uint16_t frame_len);
int send_frame_ff(uint8_t mode, const uint8_t *frame, uint16_t frame_len);
