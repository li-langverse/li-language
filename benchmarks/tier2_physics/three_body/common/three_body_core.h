// Shared 2D three-body gravity — velocity Verlet, softening.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void li_three_body_kernel(void);
double li_three_body_checksum(void);

#ifdef __cplusplus
}
#endif
