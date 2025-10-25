#ifndef MINI_VKM_UTILS_H
#define MINI_VKM_UTILS_H

#include <inttypes.h>
#include <stdlib.h>

#include "errors.h"

typedef enum CPUVendor {
    GenuineIntel = 0,
    AuthenticAMD,
} MiniKVMCPUVendor;

int32_t check_cpu_vendor(MiniKVMCPUVendor v);

MiniKVMError mini_kvm_open_vm_fs(const char *path);
int32_t mini_kvm_check_vm(char *name);

// === STRING UTILS ===

// return true is str is a number (1234 => 1, hello => 0, 1234.1234 => 0)
int32_t mini_kvm_is_uint(const char *str, size_t n);
int32_t mini_kvm_to_uint(char *str, size_t n, uint64_t *dst);

// the input list should be a comma separated list of integers (1,2,3 is a valid list)
MiniKVMError mini_kvm_parse_int_list(char *raw_list, uint64_t **list, uint64_t *list_size);
// parse a raw cpu list and convert it to a cpu mask
MiniKVMError mini_kvm_parse_cpu_list(char *raw_list, uint64_t *cpu_list);

#endif /*  MINI_VKM_UTILS_H */
