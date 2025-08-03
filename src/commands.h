#ifndef MINI_KVM_COMMANDS_H
#define MINI_KVM_COMMANDS_H

#include <inttypes.h>

typedef struct MiniKVMCommand {
    char *name;
    int (*run)(int, char **);
} MiniKVMCommand;

int32_t mini_kvm_run(int argc, char **argv);
int32_t mini_kvm_status(int argc, char **argv);

#endif /* MINI_KVM_COMMANDS_H */
