#ifndef MINI_KVM_RUN_COMMAND
#define MINI_KVM_RUN_COMMAND

#include <inttypes.h>
#include <stdbool.h>

typedef struct MiniKvmRunArgs {
    bool log_enabled;
    uint32_t vcpu;
    uint64_t mem_size;
    uint64_t kernel_size;
    uint8_t *kernel_code;
} MiniKvmRunArgs;

int mini_kvm_run(int argc, char **argv);

#endif /* MINI_KVM_RUN_COMMAND */
