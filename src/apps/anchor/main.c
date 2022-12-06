#include <stdbool.h>
#include <stddef.h>

#include <zephyr.h>
#include <zephyr/logging/log.h>

#include <uwb/uwb.h>
#include <uwb/anchor.h>

LOG_MODULE_REGISTER(main);

void main(void) {
    int res = uwb_module_initialize(UWB_TWR_MODE_SS);
    LOG_INF("init res: %d", res);
    if (res) {
        return;
    }

    unsigned count = 0;
    while (true) {
        count++;
        res = uwb_anchor_twr(CONFIG_HRTLS_PAN_ID, CONFIG_HRTLS_UWB_ADDR);
        if (res != -2) {
            LOG_INF("%u anchor twr res: %d", count++, res);
        }
    }
}
