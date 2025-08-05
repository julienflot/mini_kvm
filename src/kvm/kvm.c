#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
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

// TODO: look for every needed for this application
static const int32_t MINI_KVM_CAPS[] = {KVM_CAP_USER_MEMORY, -1};
static const char *MINI_KVM_CAPS_STR[] = {"KVM_CAP_USER_MEMORY"};

static volatile sig_atomic_t sig_status = 0;
static void set_signal_status(int signo) { sig_status = signo; }

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

void mini_kvm_set_signals() {
    if (signal(SIGINT, set_signal_status) == SIG_ERR) {
        WARN("unable to register to signal SIGINT");
    }

    if (signal(SIGTERM, set_signal_status) == SIG_ERR) {
        WARN("unable to register to signal SIGINT");
    }
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

MiniKVMError mini_kvm_vcpu_run(Kvm *kvm, int32_t id) {
    VCpu *vcpu = NULL;
    int32_t shutdown = 0;
    MiniKVMError ret = 0;

    if (kvm == NULL || ((uint32_t)id > kvm->vcpu_count)) {
        return MINI_KVM_INTERNAL_ERROR;
    }

    vcpu = &kvm->vcpus[id];
    kvm->state = MINI_KVM_RUNNING;

    mini_kvm_set_signals();
    INFO("starting running vm");
    while (!shutdown) {
        vcpu->running = 1;
        ret = ioctl(vcpu->fd, KVM_RUN);
        vcpu->running = 0;
        if (ret < 0) {
            ERROR("failed to run VM (%s)", strerror(errno));
            ret = MINI_KVM_FAILED_RUN;
            shutdown = 1;
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
            shutdown = 1;
            break;
        case KVM_EXIT_INTERNAL_ERROR:
            ERROR("KVM: exit internal error");
            break;
        case KVM_EXIT_FAIL_ENTRY:
            ERROR("KVM: exit failed entry");
            shutdown = 1;
            break;
        case KVM_EXIT_UNKNOWN:
            ERROR("KVM: exit unknown");
            shutdown = 1;
            break;
        default:
            TRACE("KVM: exit unhandled %d", exit_reason);
            break;
        }

        if (sig_status == SIGINT || sig_status == SIGTERM) {
            shutdown = 1;
        }
    }

    return MINI_KVM_SUCCESS;
}

void mini_kvm_clean_kvm(Kvm *kvm) {
    if (kvm->vcpu_count > 0) {
        for (uint32_t i = 0; i < kvm->vcpu_count; i++) {
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
