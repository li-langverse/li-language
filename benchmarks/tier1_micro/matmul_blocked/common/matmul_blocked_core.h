#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void li_matmul_blocked_kernel(void);
double li_matmul_blocked_checksum(void);

#ifdef __cplusplus
}
#endif
