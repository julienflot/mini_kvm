#ifndef MINI_KVM_STRUCT
#define MINI_KVM_STRUCT

#include <asm/kvm.h>
#include <inttypes.h>
#include <linux/kvm.h>

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
    int32_t kvm_fd;
    int32_t vm_fd;

    int64_t mem_size;
    uint64_t *mem;
    struct kvm_userspace_memory_region u_region;

    uint32_t vcpu_count;
    uint32_t vcpu_capacity;
    VCpu *vcpus;
} Kvm;

int32_t mini_kvm_setup_kvm(Kvm *kvm, uint32_t mem_size);
void mini_kvm_set_signals();
void mini_kvm_clean_kvm(Kvm *kvm);
int32_t mini_kvm_add_vcpu(Kvm *kvm);
int32_t mini_kvm_setup_vcpu(Kvm *kvm, uint32_t id);
int32_t mini_kvm_vcpu_run(Kvm *kvm, int32_t id);

#endif /* MINI_KVM_STRUCT */
