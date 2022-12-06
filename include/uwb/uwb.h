#pragma once

enum uwb_twr_mode {
    UWB_TWR_MODE_SS,
    UWB_TWR_MODE_DS
};

extern enum uwb_twr_mode uwb_current_mode;
int uwb_module_initialize(enum uwb_twr_mode mode);
