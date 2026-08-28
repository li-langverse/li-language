#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LI_MAX_THREADS 64

void li_parallel_for_i64(long long start, long long end, void (*body)(long long), int team_size);

#ifdef __cplusplus
}
#endif
