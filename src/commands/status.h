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

typedef struct MiniKvmStatusArgs {
    char *name;
    bool regs;
    uint64_t vcpus;
} MiniKvmStatusArgs;

typedef enum MiniKvmStatusCommandType {
    MINI_KVM_COMMAND_NONE = 0,
    MINI_KVM_COMMAND_PAUSE,
    MINI_KVM_COMMAND_RESUME,
    MINI_KVM_COMMAND_SHOW_STATE,
    MINI_KVM_COMMAND_SHOW_REGS
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

MiniKVMError mini_kvm_status_create_socket(Kvm *kvm, struct sockaddr_un *addr);
int32_t mini_kvm_status_receive_cmd(Kvm *kvm);
MiniKVMError mini_kvm_status_handle_command(Kvm *kvm, MiniKvmStatusCommand *cmd,
                                            MiniKvmStatusResult *res);

#endif /* MINI_KVM_STATUS */
