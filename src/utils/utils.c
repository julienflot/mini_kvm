#include "utils.h"

#include "utils/errors.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define EAX 0
#define EBX 1
#define ECX 2
#define EDX 3
#define VENDOR_ID_LEN 13

static uint32_t cpuid[4] = {0};
static const char *vendor_name[] = {"GenuineIntel", "AuthenticAMD"};

void native_cpuid(int32_t function, uint32_t *out) {
    out[EAX] = function;
    out[ECX] = 0;

    asm volatile("cpuid"
                 : "=a"(out[EBX]), "=b"(out[EBX]), "=c"(out[ECX]), "=d"(out[EDX])
                 : "0"(out[EAX]), "2"(out[ECX])
                 : "memory");
}

int check_cpu_vendor(MiniKVMCPUVendor v) {
    char name[VENDOR_ID_LEN];
    native_cpuid(0, cpuid);

    ((uint32_t *)name)[0] = cpuid[EBX];
    ((uint32_t *)name)[1] = cpuid[EDX];
    ((uint32_t *)name)[2] = cpuid[ECX];
    name[VENDOR_ID_LEN - 1] = '\0';

    return strncmp(vendor_name[v], name, VENDOR_ID_LEN) == 0;
}

int32_t mini_kvm_is_number(const char *str, size_t n) {
    if (str == NULL || strlen(str) == 0) {
        return 0;
    }

    size_t index = 0;
    while (str[index] != '\0' && index < n) {
        if (str[index] < '0' || str[index] > '9') {
            return 0;
        }
        index++;
    }

    return 1;
}

int32_t mini_kvm_to_number(const char *str, size_t n, uint64_t *dst) {
    if (!mini_kvm_is_number(str, n)) {
        return -1;
    }

    int32_t index = n - 1;
    size_t exponent = 1;
    while (index >= 0) {
        *dst += ((uint32_t)(str[index] - '0') * exponent);
        index--;
        exponent *= 10;
    }

    return 0;
}

int32_t mini_kvm_parse_cpu_list(char *raw_list, uint64_t *cpu_list) {
    int32_t index = 0, ret = MINI_KVM_SUCCESS;
    uint64_t raw_list_len = strlen(raw_list), final = 0;

    if (raw_list == NULL || raw_list[0] == '\0') {
        return ret;
    }

    while (raw_list[index] != '\0') {
        uint32_t offset = 1, current = 0;
        while (index + offset < raw_list_len && mini_kvm_is_number(raw_list + index, offset)) {
            offset++;
        }

        // check if we have valid character in the list
        if (index + offset < raw_list_len && raw_list[index + offset - 1] != ',') {
            ret = MINI_KVM_ARGS_FAILED;
            return ret;
        }

        ret = mini_kvm_to_number(raw_list + index, offset - ((raw_list_len - (index + offset)) > 0),
                                 (uint64_t *)&current);
        if (ret != MINI_KVM_SUCCESS) {
            return ret;
        }

        final |= 1 << current;
        index += offset;
    }

    *cpu_list = final;
    return ret;
}
