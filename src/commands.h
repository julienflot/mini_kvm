#ifndef MINI_KVM_COMMANDS_H
#define MINI_KVM_COMMANDS_H

#include "utils/errors.h"
#include <inttypes.h>
#include <stdint.h>

typedef struct MiniKVMCommand {
    char *name;
    MiniKVMError (*run)(int32_t, char **);
} MiniKVMCommand;

MiniKVMError mini_kvm_run(int argc, char **argv);
MiniKVMError mini_kvm_status(int argc, char **argv);

#endif /* MINI_KVM_COMMANDS_H */
