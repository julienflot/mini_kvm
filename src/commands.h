#ifndef MINI_KVM_COMMANDS_H
#define MINI_KVM_COMMANDS_H

typedef struct MiniKVMCommand {
    char *name;
    int (*run)(int, char **);
} MiniKVMCommand;

#endif /* MINI_KVM_COMMANDS_H */
