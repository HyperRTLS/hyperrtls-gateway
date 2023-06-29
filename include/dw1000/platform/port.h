#pragma once
#include <stdint.h>
#include <string.h>
typedef uint64_t        uint64 ;
typedef int64_t         int64 ;

void Sleep(uint32_t Delay);

void port_set_dw1000_slowrate(void);
void port_set_dw1000_fastrate(void);

void reset_DW1000(void);
