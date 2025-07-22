#ifndef MINI_VKM_UTILS_H
#define MINI_VKM_UTILS_H

#include <stdint.h>
#include <stdlib.h>

typedef enum CPUVendor {
    GenuineIntel = 0,
    AuthenticAMD,
} MiniKVMCPUVendor;

int check_cpu_vendor(MiniKVMCPUVendor v);

// === STRING UTILS ===

// return true is str is a number (1234 => 1, hello => 0, 1234.1234 => 0)
int32_t is_number(const char *str, size_t n);
int32_t to_number(const char *str, size_t n, uint32_t *dst);

#endif /*  MINI_VKM_UTILS_H */
