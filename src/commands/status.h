#ifndef MINI_KVM_STATUS
#define MINI_KVM_STATUS

#include <inttypes.h>
#include <linux/kvm.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "constants.h"
#include "kvm/kvm.h"
#include "utils/errors.h"

typedef enum MiniKvmStatusCommandType {
    MINI_KVM_COMMAND_NONE = 0,
    MINI_KVM_COMMAND_PAUSE,
    MINI_KVM_COMMAND_RESUME,
    MINI_KVM_COMMAND_SHUTDOWN,
    MINI_KVM_COMMAND_SHOW_STATE,
    MINI_KVM_COMMAND_SHOW_REGS,
    MINI_KVM_COMMAND_DUMP_MEM,
    MINI_KVM_COMMAND_COUNT,
} MiniKvmStatusCommandType;

typedef struct MiniKvmStatusArgs {
    char *name;
    bool regs;
    uint64_t *mem_range;
    uint64_t mem_range_size;
    uint64_t vcpus;
    uint64_t cmd_count;
    MiniKvmStatusCommandType cmds[MINI_KVM_COMMAND_COUNT];
} MiniKvmStatusArgs;

typedef struct MiniKvmStatusCommand {
    MiniKvmStatusCommandType type;
    uint64_t vcpus;
    int64_t mem_range[4];
    int32_t pid;
} MiniKvmStatusCommand;

typedef struct MiniKvmStatusResult {
    MiniKvmStatusCommandType cmd_type;
    MiniKVMError error;
    uint64_t vcpus;
    struct kvm_regs regs[MINI_KVM_MAX_VCPUS];
    struct kvm_sregs sregs[MINI_KVM_MAX_VCPUS];
    VMState state;
} MiniKvmStatusResult;

MiniKVMError mini_kvm_status_handle_command(Kvm *kvm, MiniKvmStatusCommand *cmd,
                                            MiniKvmStatusResult *res);

#endif /* MINI_KVM_STATUS */
