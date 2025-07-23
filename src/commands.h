#ifndef MINI_KVM_COMMANDS_H
#define MINI_KVM_COMMANDS_H

typedef struct MiniKVMCommand {
    char *name;
    int (*run)(int, char **);
} MiniKVMCommand;

int mini_kvm_run(int argc, char **argv);
int mini_kvm_status(int argc, char **argv);

#endif /* MINI_KVM_COMMANDS_H */
