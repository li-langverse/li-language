#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void li_panic(const char* msg);
void li_bounds_fail(void);
void li_rt_print_int(int32_t value);
void li_rt_print_str(const char* s);
void li_rt_set_args(int argc, char** argv);
int li_rt_argc(void);
const char* li_rt_argv(int index);
