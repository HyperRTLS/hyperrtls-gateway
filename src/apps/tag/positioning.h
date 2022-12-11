#pragma once
#include <stddef.h>

#include "rtls/rtls.h"

int perform_positioning(struct rtls_result *out_result, size_t repetitions);
