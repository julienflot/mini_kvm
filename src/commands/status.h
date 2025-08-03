#ifndef MINI_KVM_STATUS
#define MINI_KVM_STATUS

#include <inttypes.h>
#include <stdbool.h>

#include "kvm/kvm.h"

typedef struct MiniKvmStatusArgs {
    char *name;
} MiniKvmStatusArgs;

typedef enum MiniKvmStatusCommandType {
    MINI_KVM_COMMAND_NONE = 0,
    MINI_KVM_COMMAND_RUNNING
} MiniKvmStatusCommandType;

typedef struct MiniKvmStatusCommand {
    MiniKvmStatusCommandType type;
} MiniKvmStatusCommand;

typedef struct MiniKvmStatusResult {
    bool running;
} MiniKvmStatusResult;

int32_t mini_kvm_start_status_thread(Kvm *kvm);
int32_t
mini_kvm_status_handle_command(Kvm *kvm, MiniKvmStatusCommand *cmd, MiniKvmStatusResult *res);

#endif /* MINI_KVM_STATUS */
