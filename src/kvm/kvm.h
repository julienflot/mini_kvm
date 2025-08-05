#ifndef MINI_KVM_STRUCT
#define MINI_KVM_STRUCT

#include "utils/errors.h"
#include <inttypes.h>
#include <linux/kvm.h>
#include <pthread.h>
#include <stdbool.h>

typedef enum VMState { MINI_KVM_PAUSED = 0, MINI_KVM_RUNNING } VMState;

typedef struct VCpu {
    int32_t fd;
    uint32_t id;

    int32_t mem_region_size;
    struct kvm_run *kvm_run;

    struct kvm_regs regs;
    struct kvm_sregs sregs;

    int32_t running;
} VCpu;

typedef struct Kvm {
    char *name;
    char *fs_path;
    int32_t fs_fd;

    int32_t kvm_fd;
    int32_t vm_fd;

    int64_t mem_size;
    uint64_t *mem;
    struct kvm_userspace_memory_region u_region;

    uint32_t vcpu_count;
    uint32_t vcpu_capacity;
    VCpu *vcpus;

    pthread_mutex_t lock;
    pthread_t status_thread;
    bool shutdown_status_thread;

    VMState state;
} Kvm;

MiniKVMError mini_kvm_setup_kvm(Kvm *kvm, uint32_t mem_size);
void mini_kvm_set_signals();
void mini_kvm_clean_kvm(Kvm *kvm);
MiniKVMError mini_kvm_add_vcpu(Kvm *kvm);
MiniKVMError mini_kvm_setup_vcpu(Kvm *kvm, uint32_t id);
MiniKVMError mini_kvm_vcpu_run(Kvm *kvm, int32_t id);

const char *mini_kvm_vm_state_str(VMState state);
void mini_kvm_print_regs(struct kvm_regs *regs);
void mini_kvm_print_sregs(struct kvm_sregs *sregs);

#endif /* MINI_KVM_STRUCT */
