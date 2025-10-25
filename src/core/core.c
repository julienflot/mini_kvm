#include "core.h"

#include "constants.h"
#include "core/containers.h"
#include "errors.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
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

int32_t check_cpu_vendor(MiniKVMCPUVendor v) {
    char name[VENDOR_ID_LEN];
    native_cpuid(0, cpuid);

    ((uint32_t *)name)[0] = cpuid[EBX];
    ((uint32_t *)name)[1] = cpuid[EDX];
    ((uint32_t *)name)[2] = cpuid[ECX];
    name[VENDOR_ID_LEN - 1] = '\0';

    return strncmp(vendor_name[v], name, VENDOR_ID_LEN) == 0;
}

MiniKVMError mini_kvm_open_vm_fs(const char *path) {
    int32_t root_fs_dir = open(MINI_KVM_FS_ROOT_PATH, O_DIRECTORY | O_RDONLY);
    if (root_fs_dir < 0) {
        return -MINI_KVM_INTERNAL_ERROR;
    }
    int32_t vm_fs_dir = openat(root_fs_dir, path, O_DIRECTORY | O_RDONLY);
    if (vm_fs_dir < 0) {
        return -MINI_KVM_INTERNAL_ERROR;
    }

    return vm_fs_dir;
}

int32_t mini_kvm_check_vm(char *name) {
    int32_t vm_dir = -1, pidfile = -1, vm_pid = -1, bytes_read = 0;
    char *pidfile_name = NULL;

    vm_dir = mini_kvm_open_vm_fs(name);
    if (vm_dir < 0) {
        return -1;
    }

    pidfile_name = malloc(sizeof(char) * (strlen(name) + 5));
    sprintf(pidfile_name, "%s.pid", name);
    pidfile = openat(vm_dir, pidfile_name, O_RDONLY);
    if (pidfile < 0) {
        return -1;
    }

    vm_pid = -1;
    bytes_read = read(pidfile, &vm_pid, sizeof(int32_t));
    if (bytes_read <= 0 || kill(vm_pid, 0) != 0) {
        return -1;
    }

    return MINI_KVM_SUCCESS;
}

static inline int32_t is_digit(char c) { return c >= '0' && c <= '9'; }

int32_t mini_kvm_is_uint(const char *str, size_t n) {
    if (str == NULL || strlen(str) == 0 || n == 0) {
        return 1;
    }

    size_t index = 0;
    while (str[index] != '\0' && index < n) {
        if (!is_digit(str[index])) {
            return 0;
        }
        index++;
    }

    return 1;
}

int32_t mini_kvm_to_uint(char *str, size_t n, uint64_t *dst) {
    if (!mini_kvm_is_uint(str, n)) {
        return -1;
    }

    char *str_end = str + n;
    *dst = strtoul(str, &str_end, 10);
    if (errno == EINVAL || errno == ERANGE) {
        return -1;
    }

    return 0;
}

MiniKVMError mini_kvm_parse_int_list(char *raw_list, vec_uint64_t **list) {
    MiniKVMError ret = MINI_KVM_SUCCESS;
    uint64_t index = 0, raw_list_len = strlen(raw_list);

    *list = vec_new_uint64_t();
    vec_resize(*list, 4);

    while (raw_list[index] != '\0') {
        uint64_t offset = 1, current = 0;
        while (index + offset < raw_list_len && mini_kvm_is_uint(raw_list + index, offset)) {
            offset++;
        }

        // check if we have valid character in the list
        if (index + offset < raw_list_len && raw_list[index + offset - 1] != ',') {
            ret = MINI_KVM_ARGS_FAILED;
            return ret;
        }

        ret = mini_kvm_to_uint(raw_list + index, offset - ((raw_list_len - (index + offset)) > 0),
                               (uint64_t *)&current);
        if (ret != MINI_KVM_SUCCESS) {
            return ret;
        }

        vec_append(*list, current);
        index += offset;
    }

    return ret;
}

MiniKVMError mini_kvm_parse_cpu_list(char *raw_list, uint64_t *cpu_list) {
    MiniKVMError ret = MINI_KVM_SUCCESS;
    vec_uint64_t *list = NULL;

    if (raw_list == NULL || raw_list[0] == '\0') {
        return ret;
    }
    if (mini_kvm_parse_int_list(raw_list, &list) != MINI_KVM_SUCCESS) {
        return MINI_KVM_INTERNAL_ERROR;
    }

    for (uint32_t i = 0; i < list->len; i++) {
        *cpu_list |= 1 << list->tab[i];
    }

    free(list);
    return ret;
}
