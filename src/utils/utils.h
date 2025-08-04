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
int32_t mini_kvm_is_number(const char *str, size_t n);
int32_t mini_kvm_to_number(const char *str, size_t n, uint64_t *dst);

// parse a raw cpu list and convert it to a cpu mask
// the input list should a comma separated list of integers (1,2,3 is a valid list)
int32_t mini_kvm_parse_cpu_list(char *raw_list, uint64_t *cpu_list);

#endif /*  MINI_VKM_UTILS_H */
