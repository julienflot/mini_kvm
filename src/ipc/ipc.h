#ifndef MINI_KVM_IPC_H
#define MINI_KVM_IPC_H

#include <inttypes.h>
#include <sys/un.h>

#include "commands/status.h"

// server side functions
int32_t mini_kvm_ipc_create_socket(Kvm *kvm, struct sockaddr_un *addr);
int32_t mini_kvm_ipc_receive_cmd(Kvm *kvm);

// client side functions
int32_t mini_kvm_ipc_connect(char *name, struct sockaddr_un *addr);
int32_t mini_kvm_ipc_send_cmd(int32_t sock, MiniKvmStatusCommand *cmd, MiniKvmStatusResult *res);

#endif /* MINI_KVM_IPC_H */
