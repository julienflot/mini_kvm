#ifndef MINI_KVM_STATUS
#define MINI_KVM_STATUS

#include <inttypes.h>
#include <linux/kvm.h>
#include <stdbool.h>

#include "constants.h"
#include "kvm/kvm.h"
#include "utils/errors.h"

typedef struct MiniKvmStatusArgs {
    char *name;
    bool regs;
    uint64_t vcpus;
} MiniKvmStatusArgs;

typedef enum MiniKvmStatusCommandType {
    MINI_KVM_COMMAND_NONE = 0,
    MINI_KVM_COMMAND_RUNNING,
    MINI_KVM_COMMAND_REGS
} MiniKvmStatusCommandType;

typedef struct MiniKvmStatusCommand {
    MiniKvmStatusCommandType type;
    uint64_t vcpus;
} MiniKvmStatusCommand;

typedef struct MiniKvmStatusResult {
    MiniKvmStatusCommandType cmd_type;
    uint64_t vcpus;
    struct kvm_regs regs[MINI_KVM_MAX_VCPUS];
    struct kvm_sregs sregs[MINI_KVM_MAX_VCPUS];
    VMState state;
} MiniKvmStatusResult;

MiniKVMError mini_kvm_start_status_thread(Kvm *kvm);
MiniKVMError mini_kvm_status_handle_command(Kvm *kvm, MiniKvmStatusCommand *cmd,
                                            MiniKvmStatusResult *res);

#endif /* MINI_KVM_STATUS */
