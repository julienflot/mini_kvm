#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "kvm.h"
#include "utils/errors.h"
#include "utils/logger.h"
#include "utils/utils.h"

#define TSS_ADDR 0xfffbd000

struct VcpuRunArgs {
    Kvm *kvm;
    VCpu *vcpu;
};

// TODO: look for every needed for this application
static const int32_t MINI_KVM_CAPS[] = {KVM_CAP_USER_MEMORY, -1};
static const char *MINI_KVM_CAPS_STR[] = {"KVM_CAP_USER_MEMORY"};

static const char *VM_STATE_STR[] = {"paused", "running", "shutdown"};

MiniKVMError mini_kvm_setup_kvm(Kvm *kvm, uint32_t mem_size) {
    int32_t kvm_version;

    kvm->kvm_fd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm->kvm_fd < 0) {
        ERROR("failed to open kvm device : s%", strerror(errno));
        return MINI_KVM_NO_DEVICE;
    }
    INFO("/dev/kvm device opened");

    if ((kvm_version = ioctl(kvm->kvm_fd, KVM_GET_API_VERSION, 0)) != KVM_API_VERSION) {
        ERROR("wrong kvm api version expected %d, got %d", kvm_version, KVM_API_VERSION);
        return MINI_KVM_WRONG_VERSION;
    }

    kvm->vm_fd = ioctl(kvm->kvm_fd, KVM_CREATE_VM, 0);
    if (kvm->vm_fd < 0) {
        ERROR("failed to create Virtual machine file descriptor : %s", strerror(errno));
        return MINI_KVM_FAILED_VM_CREATION;
    }
    INFO("KVM virtual machine created");

    for (uint32_t i = 0; MINI_KVM_CAPS[i] != -1; i++) {
        if ((ioctl(kvm->kvm_fd, KVM_CHECK_EXTENSION, MINI_KVM_CAPS[i])) < 0) {
            ERROR("kvm capabilites unsupported : %s", MINI_KVM_CAPS_STR[i]);
            return MINI_KVM_UNSUPPORTED_CAPS;
        }
    }

    if (check_cpu_vendor(GenuineIntel)) {
        INFO("Running on an Intel CPU, set TSS addr to 0x%llx", TSS_ADDR);
        if (ioctl(kvm->kvm_fd, KVM_SET_TSS_ADDR, TSS_ADDR), 0) {
            ERROR("failed to set TSS ADDR : %s", strerror(errno));
            return MINI_KVM_FAILED_IOCTL;
        }
    }

    if (mem_size == 0) {
        ERROR("cannot create VM with memory of size 0");
        return MINI_KVM_FAILED_ALLOCATION;
    }
    kvm->mem_size = mem_size;
    kvm->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (kvm->mem == MAP_FAILED) {
        ERROR("failed to allocate VM memory (%s)", strerror(errno));
        return MINI_KVM_FAILED_ALLOCATION;
    }
    INFO("VM memory allocated");

    kvm->u_region.slot = 0;
    kvm->u_region.flags = 0;
    kvm->u_region.guest_phys_addr = 0;
    kvm->u_region.memory_size = kvm->mem_size;
    kvm->u_region.userspace_addr = (uint64_t)kvm->mem;
    if (ioctl(kvm->vm_fd, KVM_SET_USER_MEMORY_REGION, &kvm->u_region) < 0) {
        ERROR("kvm: failed to set use memory region (%s)", strerror(errno));
        return MINI_KVM_FAILED_MEMORY_REGION_CREATION;
    }
    INFO("VM memory region created at guest physical address 0x0");

    kvm->vcpu_count = 0;
    kvm->vcpu_capacity = 1;
    kvm->vcpus = malloc(sizeof(VCpu));

    pthread_mutex_init(&kvm->lock, NULL);
    kvm->state = MINI_KVM_PAUSED;

    return MINI_KVM_SUCCESS;
}

static void mini_kvm_append_vcpu(Kvm *kvm, VCpu *vcpu) {
    if (vcpu == NULL) {
        ERROR("cannot create append vcpu VCPU without initializing KVM");
        return;
    }

    if (kvm->vcpu_count >= kvm->vcpu_capacity) {
        kvm->vcpu_capacity = kvm->vcpu_capacity << 1;
        VCpu *vcpus = malloc(sizeof(VCpu) * kvm->vcpu_capacity);

        memcpy(vcpus, kvm->vcpus, sizeof(VCpu) * kvm->vcpu_count);
        free(kvm->vcpus);
        kvm->vcpus = vcpus;
    }

    kvm->vcpus[vcpu->id] = *vcpu;
    kvm->vcpu_count++;
}

MiniKVMError mini_kvm_add_vcpu(Kvm *kvm) {
    if (kvm == NULL) {
        ERROR("cannot create vcpu VCPU without initializing KVM");
        return MINI_KVM_FAILED_VCPU_CREATION;
    }

    VCpu vcpu = {0};
    vcpu.id = kvm->vcpu_count;
    vcpu.fd = ioctl(kvm->vm_fd, KVM_CREATE_VCPU, vcpu.id);
    if (vcpu.fd < 0) {
        ERROR("failed to create vcpu %d (%s)", vcpu.id, strerror(errno));
        return MINI_KVM_FAILED_VCPU_CREATION;
    }

    vcpu.mem_region_size = ioctl(kvm->kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (vcpu.mem_region_size <= 0) {
        ERROR("failed to get vcpu mem size (%s)", strerror(errno));
        return MINI_KVM_FAILED_VCPU_CREATION;
    }

    vcpu.kvm_run = mmap(NULL, vcpu.mem_region_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu.fd, 0);
    if (vcpu.kvm_run == MAP_FAILED) {
        ERROR("failed to create kvm run struct for vcpu %d (%s)", vcpu.id, strerror(errno));
        return MINI_KVM_FAILED_VCPU_CREATION;
    }

    mini_kvm_append_vcpu(kvm, &vcpu);
    INFO("VCPU %d initialized", vcpu.id);

    return MINI_KVM_SUCCESS;
}

MiniKVMError mini_kvm_setup_vcpu(Kvm *kvm, uint32_t id) {
    VCpu *vcpu = NULL;
    int32_t ret = 0;

    if (id > kvm->vcpu_count) {
        return MINI_KVM_INTERNAL_ERROR;
    }

    vcpu = &kvm->vcpus[id];

    memset(&vcpu->regs, 0, sizeof(struct kvm_regs));
    vcpu->regs.rip = 0;
    vcpu->regs.rsp = kvm->mem_size - 1;
    vcpu->regs.rflags = 0b01;
    ret = ioctl(vcpu->fd, KVM_SET_REGS, &vcpu->regs);
    if (ret < 0) {
        ERROR("failed to set vcpu %d regs (%s)", vcpu->id, strerror(errno));
        return MINI_KVM_FAILED_VCPU_CREATION;
    }
    INFO("VCPU %d regs set", id);

    ret = ioctl(vcpu->fd, KVM_GET_SREGS, &vcpu->sregs);
    if (ret < 0) {
        ERROR("failed to get vcpu %d sregs (%s)", vcpu->id, strerror(errno));
        return MINI_KVM_FAILED_VCPU_CREATION;
    }

    vcpu->sregs.cs.selector = 0;
    vcpu->sregs.cs.base = 0;
    vcpu->sregs.ds.selector = 0;
    vcpu->sregs.ds.base = 0;
    vcpu->sregs.ss.selector = 0;
    vcpu->sregs.ss.base = 0;

    ret = ioctl(vcpu->fd, KVM_SET_SREGS, &vcpu->sregs);
    if (ret < 0) {
        ERROR("failed to set vcpu %d sregs (%s)", vcpu->id, strerror(errno));
        return MINI_KVM_FAILED_VCPU_CREATION;
    }
    INFO("VCPU %d sregs set", id);

    return MINI_KVM_SUCCESS;
}

MiniKVMError mini_kvm_start_vm(Kvm *kvm) {
    MiniKVMError ret = MINI_KVM_SUCCESS;

    if (kvm == NULL || kvm->vcpu_count == 0) {
        return MINI_KVM_INTERNAL_ERROR;
    }

    for (uint32_t vcpu_index = 0; vcpu_index < kvm->vcpu_count; vcpu_index++) {
        ret = mini_kvm_vcpu_run(kvm, vcpu_index);
        if (ret != MINI_KVM_SUCCESS) {
            break;
        }
    }

    INFO("starting running vm");
    kvm->state = MINI_KVM_RUNNING;
    return ret;
}

static void *kvm_vcpu_thread_run(void *args) {
    struct VcpuRunArgs *vcpu_args = (struct VcpuRunArgs *)args;
    Kvm *kvm = vcpu_args->kvm;
    VCpu *vcpu = vcpu_args->vcpu;
    MiniKVMError ret = 0;

    while (kvm->state != MINI_KVM_SHUTDOWN) {

        if (kvm->state == MINI_KVM_PAUSED) {
            usleep(10000);
            continue;
        }

        vcpu->running = 1;
        ret = ioctl(vcpu->fd, KVM_RUN);
        vcpu->running = 0;
        if (ret < 0) {
            ERROR("failed to run VM (%s)", strerror(errno));
            ret = MINI_KVM_FAILED_RUN;
            kvm->state = MINI_KVM_SHUTDOWN;
        }

        int32_t exit_reason = vcpu->kvm_run->exit_reason;
        switch (exit_reason) {
        case KVM_EXIT_DEBUG:
            TRACE("KVM: exit debug");
            break;
        case KVM_EXIT_HLT:
            TRACE("KVM: exit hlt");
            break;
        case KVM_EXIT_IO:
            TRACE("KVM: exit io");
            sleep(1);
            break;
        case KVM_EXIT_MMIO:
            TRACE("KVM: exit mmio");
            break;
        case KVM_EXIT_INTR:
            TRACE("KVM: exit intr");
            break;
        case KVM_EXIT_SHUTDOWN:
            ERROR("KVM: exit shutdown");
            kvm->state = MINI_KVM_SHUTDOWN;
            break;
        case KVM_EXIT_INTERNAL_ERROR:
            ERROR("KVM: exit internal error");
            break;
        case KVM_EXIT_FAIL_ENTRY:
            ERROR("KVM: exit failed entry");
            kvm->state = MINI_KVM_SHUTDOWN;
            break;
        case KVM_EXIT_UNKNOWN:
            ERROR("KVM: exit unknown");
            kvm->state = MINI_KVM_SHUTDOWN;
            break;
        default:
            TRACE("KVM: exit unhandled %d", exit_reason);
            break;
        }
    }
    return NULL;
}

MiniKVMError mini_kvm_vcpu_run(Kvm *kvm, int32_t id) {
    MiniKVMError ret = MINI_KVM_SUCCESS;
    struct VcpuRunArgs *args = malloc(sizeof(struct VcpuRunArgs));
    VCpu *vcpu = &kvm->vcpus[id];

    args->kvm = kvm;
    args->vcpu = vcpu;
    ret = pthread_create(&vcpu->thread, NULL, kvm_vcpu_thread_run, args);
    if (ret != 0) {
        ERROR("unable to create thread for vcpu %d", id);
        ret = MINI_KVM_FAILED_RUN;
    }

    return ret;
}

void mini_kvm_clean_kvm(Kvm *kvm) {
    if (kvm->vcpu_count > 0) {
        for (uint32_t i = 0; i < kvm->vcpu_count; i++) {
            VCpu vcpu = kvm->vcpus[i];
            if (vcpu.thread) {
                pthread_join(kvm->vcpus[i].thread, NULL);
            }
            munmap(kvm->vcpus[i].kvm_run, kvm->vcpus[i].mem_region_size);
            close(kvm->vcpus[i].fd);
        }
        free(kvm->vcpus);
    }

    if (kvm->name) {
        close(kvm->fs_fd);
        free(kvm->fs_path);
        free(kvm->name);
    }

    if (kvm->mem) {
        munmap(kvm->mem, kvm->mem_size);
    }

    close(kvm->kvm_fd);
    close(kvm->vm_fd);
    free(kvm);
}

const char *mini_kvm_vm_state_str(VMState state) { return VM_STATE_STR[state]; }

void mini_kvm_print_regs(struct kvm_regs *regs) {
    fprintf(stdout, "rax 0x%016llx\trbx 0x%016llx\trcx 0x%016llx\trdx 0x%016llx\n", regs->rax,
            regs->rbx, regs->rcx, regs->rdx);
    fprintf(stdout, "r8  0x%016llx\tr9  0x%016llx\tr10 0x%016llx\tr11 0x%016llx\n", regs->r8,
            regs->r9, regs->r10, regs->r11);
    fprintf(stdout, "r12 0x%016llx\tr13 0x%016llx\tr14 0x%016llx\tr15 0x%016llx\n", regs->r12,
            regs->r13, regs->r14, regs->r15);
    fprintf(stdout, "rsp 0x%016llx\trbp 0x%016llx\trip 0x%016llx\trflags 0x%016llx\n", regs->rsp,
            regs->rbp, regs->rip, regs->rflags);
    fprintf(stdout, "rdi 0x%016llx\trsi 0x%016llx\n", regs->rdi, regs->rsi);
}

void mini_kvm_print_sregs(struct kvm_sregs *sregs) {
    fprintf(stdout, "cr0 0x%016llx\tcr2 0x%016llx\tcr3 0x%016llx\tcr4 0x%016llx\n", sregs->cr0,
            sregs->cr2, sregs->cr3, sregs->cr4);
}
