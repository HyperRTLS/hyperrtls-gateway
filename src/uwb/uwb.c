#include <dw1000/decadriver/deca_device_api.h>
#include <dw1000/platform/deca_spi.h>
#include <dw1000/platform/port.h>

#include <zephyr.h>

#include <uwb/uwb.h>

#define INIT_TIMEOUT_MS 2000

// borrowed from decawave samples
#define TX_ANT_DLY 16436
#define RX_ANT_DLY 16436

static dwt_config_t config_ss = {
    2,               /* Channel number. */
    DWT_PRF_64M,     /* Pulse repetition frequency. */
    DWT_PLEN_128,    /* Preamble length. Used in TX only. */
    DWT_PAC8,        /* Preamble acquisition chunk size. Used in RX only. */
    9,               /* TX preamble code. Used in TX only. */
    9,               /* RX preamble code. Used in RX only. */
    0,               /* 0 to use standard SFD, 1 to use non-standard SFD. */
    DWT_BR_6M8,      /* Data rate. */
    DWT_PHRMODE_STD, /* PHY header mode. */
    (129 + 8 - 8)    /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
};

static dwt_config_t config_ds = {
    2,               /* Channel number. */
    DWT_PRF_64M,     /* Pulse repetition frequency. */
    DWT_PLEN_1024,   /* Preamble length. Used in TX only. */
    DWT_PAC32,       /* Preamble acquisition chunk size. Used in RX only. */
    9,               /* TX preamble code. Used in TX only. */
    9,               /* RX preamble code. Used in RX only. */
    1,               /* 0 to use standard SFD, 1 to use non-standard SFD. */
    DWT_BR_110K,     /* Data rate. */
    DWT_PHRMODE_STD, /* PHY header mode. */
    (1025 + 64 - 32) /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
};

enum uwb_twr_mode uwb_current_mode = -1;

static bool try_initalizing(void) {
    reset_DW1000();
    port_set_dw1000_slowrate();
    return dwt_initialise(DWT_LOADUCODE) == DWT_SUCCESS;
}

int uwb_module_initialize(enum uwb_twr_mode mode) {
    int64_t start = k_uptime_get();
    bool res;

    do {
        res = try_initalizing();
    }
    while (!res && k_uptime_get() - start < INIT_TIMEOUT_MS);

    if (!res) {
        return -1;
    }

    port_set_dw1000_fastrate();
    dwt_configure(mode == UWB_TWR_MODE_SS ? &config_ss : &config_ds);
    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);

    uwb_current_mode = mode;

    return 0;
}
