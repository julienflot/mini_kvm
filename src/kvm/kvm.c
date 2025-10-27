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

#include "core/core.h"
#include "core/errors.h"
#include "core/logger.h"
#include "kvm.h"

#define TSS_ADDR 0xfffbd000
#define MAX_CPUID_ENTRIES 100

struct VcpuRunArgs {
    Kvm *kvm;
    VCpu *vcpu;
};

// TODO: look for every capabilites needed for this application
static const int32_t MINI_KVM_CAPS[] = {KVM_CAP_USER_MEMORY, KVM_CAP_SET_TSS_ADDR,
                                        KVM_CAP_EXT_CPUID, -1};
static const char *MINI_KVM_CAPS_STR[] = {"KVM_CAP_USER_MEMORY", "KVM_CAP_SET_TSS_ADDR",
                                          "KVM_CAP_EXT_CPUID"};
static const char *VM_STATE_STR[] = {"paused", "running", "shutdown"};

MiniKVMError mini_kvm_setup_kvm(Kvm *kvm, uint32_t mem_size) {
    int32_t kvm_version;

    kvm->vcpus = vec_new_VCpu();
    pthread_mutex_init(&kvm->lock, NULL);
    kvm->state = MINI_KVM_PAUSED;

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
    INFO("VM memory allocated (%lu bytes)", kvm->mem_size);

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

    return MINI_KVM_SUCCESS;
}

MiniKVMError mini_kvm_add_vcpu(Kvm *kvm) {
    if (kvm == NULL) {
        ERROR("cannot create vcpu VCPU without initializing KVM");
        return MINI_KVM_FAILED_VCPU_CREATION;
    }

    VCpu vcpu = {0};
    vcpu.id = kvm->vcpus->len;
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

    vec_append(kvm->vcpus, vcpu);
    INFO("VCPU %d initialized", vcpu.id);

    return MINI_KVM_SUCCESS;
}

static void mini_kvm_vcpu_signal_handler(int signum) {
    if (signum == SIGVMPAUSE) {
        TRACE("pause signal received");
    } else if (signum == SIGVMRESUME) {
        TRACE("resume signal received");
    }
}

static MiniKVMError kvm_setup_cpuid(Kvm *kvm, VCpu *vcpu) {
    struct kvm_cpuid2 *cpuid =
        calloc(1, sizeof(struct kvm_cpuid2) + MAX_CPUID_ENTRIES * sizeof(struct kvm_cpuid_entry2));
    cpuid->nent = MAX_CPUID_ENTRIES;

    if (ioctl(kvm->kvm_fd, KVM_GET_SUPPORTED_CPUID, cpuid) < 0) {
        ERROR("kvm: failed to get supported cpuid (%s)", strerror(errno));
        return MINI_KVM_FAILED_IOCTL;
    }

    if (ioctl(vcpu->fd, KVM_SET_CPUID2, cpuid) < 0) {
        ERROR("kvm: failed to set cpuid (%s)", strerror(errno));
        return MINI_KVM_FAILED_IOCTL;
    }

    return MINI_KVM_SUCCESS;
}

MiniKVMError mini_kvm_setup_vcpu(Kvm *kvm, uint32_t id, uint64_t start_addr) {
    VCpu *vcpu = NULL;
    int32_t ret = 0;

    if (id > kvm->vcpus->len) {
        return MINI_KVM_INTERNAL_ERROR;
    }

    vcpu = &kvm->vcpus->tab[id];

    memset(&vcpu->regs, 0, sizeof(struct kvm_regs));
    vcpu->regs.rip = start_addr;
    vcpu->regs.rsp = kvm->mem_size - 1;
    vcpu->regs.rbp = vcpu->regs.rsp;
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

    if (kvm_setup_cpuid(kvm, vcpu) != MINI_KVM_SUCCESS) {
        return MINI_KVM_FAILED_VCPU_CREATION;
    }
    INFO("VCPU %d cpuid set", vcpu->id);

    signal(SIGVMPAUSE, mini_kvm_vcpu_signal_handler);
    signal(SIGVMRESUME, mini_kvm_vcpu_signal_handler);
    signal(SIGVMSHUTDOWN, mini_kvm_vcpu_signal_handler);

    return MINI_KVM_SUCCESS;
}

static MiniKVMError mini_kvm_handle_io(struct kvm_run *kvm_run) {
    MiniKVMError ret = MINI_KVM_SUCCESS;
    char *p = (char *)kvm_run;

    if (kvm_run->io.direction == KVM_EXIT_IO_OUT) {
        switch (kvm_run->io.port) {
        case 0x3f8:
            write(1, p + kvm_run->io.data_offset, 1);
            fflush(stdout);
            break;

        default:
            ERROR("mini_kvm: unhandled out io port on port %x", kvm_run->io.port);
            ret = MINI_KVM_INTERNAL_ERROR;
            break;
        }
    }

    return ret;
}

MiniKVMError mini_kvm_start_vm(Kvm *kvm) {
    MiniKVMError ret = MINI_KVM_SUCCESS;

    if (kvm == NULL || kvm->vcpus->len == 0) {
        ERROR("0 VCPUs was configured, unable to start VM ...");
        return MINI_KVM_INTERNAL_ERROR;
    }

    for (uint32_t vcpu_index = 0; vcpu_index < kvm->vcpus->len; vcpu_index++) {
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

    vcpu->running = 1;
    while (kvm->state != MINI_KVM_SHUTDOWN) {

        // TODO: find a better to wait for the resume signal and better way to synchrnoize threads
        // on state transition
        if (kvm->state == MINI_KVM_PAUSED) {
            usleep(10000);
            continue;
        }

        ret = ioctl(vcpu->fd, KVM_RUN);
        if (ret < 0) {
            ERROR("failed to run VM (%s)", strerror(errno));
            kvm->state = MINI_KVM_SHUTDOWN;
            break;
        }

        int32_t exit_reason = vcpu->kvm_run->exit_reason;
        switch (exit_reason) {
        case KVM_EXIT_HLT:
            TRACE("KVM: exit hlt");
            kvm->state = MINI_KVM_SHUTDOWN;
            break;
        case KVM_EXIT_IO:
            if (mini_kvm_handle_io(vcpu->kvm_run) != MINI_KVM_SUCCESS) {
                kvm->state = MINI_KVM_SHUTDOWN;
            }
            break;
        case KVM_EXIT_SHUTDOWN:
            ERROR("KVM: exit shutdown");
            kvm->state = MINI_KVM_SHUTDOWN;
            break;
        case KVM_EXIT_INTERNAL_ERROR:
            ERROR("KVM: exit internal error");
            kvm->state = MINI_KVM_SHUTDOWN;
            ioctl(vcpu->fd, KVM_GET_REGS, &vcpu->regs);
            mini_kvm_print_regs(&vcpu->regs);
            break;
        case KVM_EXIT_INTR:
            TRACE("KVM: exit INTR");
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
    VCpu *vcpu = &kvm->vcpus->tab[id];

    args->kvm = kvm;
    args->vcpu = vcpu;
    ret = pthread_create(&vcpu->thread, NULL, kvm_vcpu_thread_run, args);
    if (ret != 0) {
        ERROR("unable to create thread for vcpu %d", id);
        ret = MINI_KVM_FAILED_RUN;
    }

    return ret;
}

void mini_kvm_send_sig(Kvm *kvm, int32_t signum) {
    if (kvm->vcpus->len == 0) {
        return;
    }

    for (uint32_t i = 0; i < kvm->vcpus->len; i++) {
        pthread_kill(kvm->vcpus->tab[i].thread, signum);
    }
}

void mini_kvm_clean_kvm(Kvm *kvm) {
    if (kvm->vcpus->len > 0) {
        for (uint32_t i = 0; i < kvm->vcpus->len; i++) {
            VCpu vcpu = kvm->vcpus->tab[i];
            if (vcpu.thread) {
                pthread_join(kvm->vcpus->tab[i].thread, NULL);
            }
            munmap(kvm->vcpus->tab[i].kvm_run, kvm->vcpus->tab[i].mem_region_size);
            close(kvm->vcpus->tab[i].fd);
        }
        vec_free(kvm->vcpus);
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

void mini_kvm_dump_mem(Kvm *kvm, int32_t out, uint64_t start, uint64_t end, uint32_t word_size,
                       uint32_t bytes_per_line) {
    uint32_t nb_lines = 0;
    uint8_t *start_ptr = NULL;

    // align start and end to word_size
    start = start - start % word_size;
    end = (end % word_size == 0) ? end : end - end % word_size + word_size;
    start_ptr = ((uint8_t *)kvm->mem + start);

    end = (kvm->mem_size < (int64_t)end) ? (uint64_t)kvm->mem_size : end;
    nb_lines = (end - start) / (bytes_per_line) + ((end - start) % (bytes_per_line) != 0);

    dprintf(out, "mem dump: @%lu -> @%lu\n", start, end);
    for (uint32_t line = 0; line < nb_lines; line++) {
        dprintf(out, "0x%08lx\t", start + line * (bytes_per_line));

        for (uint32_t word = 0; word < bytes_per_line; word += word_size) {
            uint32_t offset = word + bytes_per_line * line;
            if (offset + start > end) {
                break;
            }

            for (uint32_t word_offset = 0; word_offset < word_size; word_offset++) {
                dprintf(out, "%02hx", start_ptr[offset + word_offset]);
            }
            dprintf(out, " ");
        }

        dprintf(out, "\n");
    }
}
