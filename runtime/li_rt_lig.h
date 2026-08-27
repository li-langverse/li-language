#ifndef LI_RT_LIG_H
#define LI_RT_LIG_H

#include <stdint.h>

int32_t li_rt_lig_kernel_run(int32_t kernel_id, int32_t backend_id);
float li_rt_lig_kernel_last_validity_ratio(void);

#endif
