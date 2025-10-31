#ifndef MINI_KVM_STRUCT
#define MINI_KVM_STRUCT

#include <inttypes.h>
#include <linux/kvm.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

#include "core/containers.h"
#include "core/errors.h"

typedef enum VMState { MINI_KVM_PAUSED = 0, MINI_KVM_RUNNING, MINI_KVM_SHUTDOWN } VMState;

#define SIGVMPAUSE (SIGRTMIN + 0)
#define SIGVMRESUME (SIGRTMIN + 1)
#define SIGVMSHUTDOWN (SIGRTMIN + 2)

typedef struct VCpu {
    int32_t fd;
    uint32_t id;

    int32_t mem_region_size;
    struct kvm_run *kvm_run;

    struct kvm_regs regs;
    struct kvm_sregs sregs;

    pthread_t thread;
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

    vec_VCpu *vcpus;
    pthread_mutex_t lock;
    int32_t sock;

    VMState state;
    uint64_t paused;
    pthread_cond_t pause_cond;
    pthread_mutex_t pause_lock;
} Kvm;

MiniKVMError mini_kvm_setup_kvm(Kvm *kvm, uint32_t mem_size);
void mini_kvm_clean_kvm(Kvm *kvm);
MiniKVMError mini_kvm_add_vcpu(Kvm *kvm);
MiniKVMError mini_kvm_setup_vcpu(Kvm *kvm, uint32_t id, uint64_t start_addr);
MiniKVMError mini_kvm_configure_paging(Kvm *kvm);
MiniKVMError mini_kvm_start_vm(Kvm *vm);
MiniKVMError mini_kvm_vcpu_run(Kvm *kvm, int32_t id);

void mini_kvm_send_sig(Kvm *kvm, int32_t signum);
void mini_kvm_pause_vm(Kvm *kvm);
void mini_kvm_resume_vm(Kvm *kvm);

const char *mini_kvm_vm_state_str(VMState state);
void mini_kvm_print_regs(struct kvm_regs *regs);
void mini_kvm_print_sregs(struct kvm_sregs *sregs);
void mini_kvm_dump_mem(Kvm *kvm, int32_t out, uint64_t start, uint64_t end, uint32_t word_size,
                       uint32_t byte_per_line);

#endif /* MINI_KVM_STRUCT */
