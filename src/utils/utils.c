#include "utils.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

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

int32_t is_number(const char *str, size_t n) {
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

int32_t to_number(const char *str, size_t n, uint64_t *dst) {
    if (!is_number(str, n)) {
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
