#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/kvm.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "args.h"
#include "logger.h"

#define TSS_ADDR 0xfffbd000
#define MEM_SIZE 0x80000

typedef struct VirtualMachine {
    int32_t kvm_fd;
    int32_t vm_fd;
    int64_t mem_size;
    uint8_t *mem;
} VirtualMachine;

typedef struct VirtualCPU {
    int32_t fd;
    int32_t id;
    uint64_t mem_size;
    struct kvm_run *kvm_run;
    struct kvm_regs regs;
    struct kvm_sregs sregs;
} VirtualCPU;

int32_t init_vm(VirtualMachine *vm, uint64_t mem_size);
int32_t init_vcpu(const VirtualMachine *vm, VirtualCPU *vcpu, int32_t id);
int32_t init_memory(VirtualMachine *vm, const char *img_path);
int32_t setup_vcpu(VirtualCPU *vcpu);
int32_t run_vm(VirtualCPU *vcpu);

// signal handling helpers
volatile sig_atomic_t sig_status = 0;
static void set_signal_status(int signo) { sig_status = signo; }

int main(int argc, char **argv) {

    int32_t ret = 0;
    VirtualMachine vm = {};
    VirtualCPU vcpu = {};
    MiniKVMArgs *args = NULL;

    args = parse_args(argc, argv);
    if (args == NULL) {
        ret = -2;
        goto exit;
    }

    // =====================
    // === STARTS THE VM ===
    // =====================
    logger_init(args->log_file_path);
    if (signal(SIGINT, set_signal_status) == SIG_ERR) {
        ERROR("unable to register set_signal_status to SIGINT signal");
        ret = -1;
        goto args_cleanup;
    }

    if ((init_vm(&vm, MEM_SIZE)) < 0) {
        ret = -1;
        goto args_cleanup;
    }

    if ((init_vcpu(&vm, &vcpu, 0)) < 0) {
        ret = -1;
        goto vm_close;
    }

    if ((setup_vcpu(&vcpu)) < 0) {
        ret = -1;
        goto vcpu_close;
    }

    if (init_memory(&vm, args->img_path) < 0) {
        ret = -1;
        goto vcpu_close;
    }

    run_vm(&vcpu);

vcpu_close:
    close(vcpu.fd);
vm_close:
    close(vm.vm_fd);
    close(vm.kvm_fd);
args_cleanup:
    logger_stop();
    free_parse_args(args);
exit:
    return ret;
}

int32_t init_vm(VirtualMachine *vm, uint64_t mem_size) {
    int32_t ret = 0;
    struct kvm_userspace_memory_region u_region = {0};

    vm->mem_size = mem_size;
    vm->kvm_fd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (vm->kvm_fd < 0) {
        ERROR("failed to open /dev/kvm");
        ret = -1;
        goto kvm_return;
    }
    INFO("KVM opened");

    int32_t kvm_version = ioctl(vm->kvm_fd, KVM_GET_API_VERSION, 0);
    if (kvm_version != KVM_API_VERSION) {
        ERROR("expected %d version got %d (%s)", KVM_API_VERSION, kvm_version, strerror(errno));
        ret = -1;
        goto kvm_close;
    }
    INFO("KVM version: %d", kvm_version);

    vm->vm_fd = ioctl(vm->kvm_fd, KVM_CREATE_VM, 0);
    if (vm->vm_fd < 0) {
        ERROR("failed to create kvm vm fd (%s)", strerror(errno));
        ret = -1;
        goto kvm_close;
    }
    INFO("KVM vm created with fd %d", vm->vm_fd);

    if ((ioctl(vm->vm_fd, KVM_SET_TSS_ADDR, TSS_ADDR)) < 0) {
        ERROR("failed to set vm tss addr (%s)", strerror(errno));
        ret = -1;
        goto vm_close;
    }
    INFO("TSS addr set to %lu", TSS_ADDR);

    vm->mem = mmap(
        NULL, vm->mem_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1,
        0);
    if (vm->mem == MAP_FAILED) {
        ERROR("failed to allocate memory for the vm (%s)", strerror(errno));
        ret = -1;
        goto vm_close;
    }
    INFO("VM memory allocated with %luB at 0x%llx", vm->mem_size, (uint64_t)vm->mem);

    TRACE("VM set memory advice to MADV_MERGEABLE");
    madvise(vm->mem, vm->mem_size, MADV_MERGEABLE);

    u_region.slot = 0;
    u_region.flags = 0;
    u_region.guest_phys_addr = 0;
    u_region.memory_size = vm->mem_size;
    u_region.userspace_addr = (uint64_t)vm->mem;

    if (ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, &u_region) < 0) {
        ERROR("failed to set userspace region (%s)", strerror(errno));
        ret = -1;
        goto vm_close;
    }
    INFO("VM userspace region set");

    return ret;

vm_close:
    close(vm->vm_fd);
kvm_close:
    close(vm->kvm_fd);
kvm_return:
    return ret;
}

int32_t init_vcpu(const VirtualMachine *vm, VirtualCPU *vcpu, int32_t id) {
    int32_t ret = 0;

    vcpu->fd = ioctl(vm->vm_fd, KVM_CREATE_VCPU, id);
    if (vcpu->fd < 0) {
        ERROR("VM failed to create vcpu with id %d (%s)", id, strerror(errno));
        ret = -1;
        goto init_vcpu_exit;
    }
    INFO("VM vcpu %d created successfully", id);
    vcpu->id = id;

    vcpu->mem_size = ioctl(vm->kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (vcpu->mem_size <= 0) {
        ERROR(
            "KVM failed to get KVM_GET_VCPU_MMAP_SIZE, got %d (%s)", vcpu->mem_size,
            strerror(errno));
        ret = -1;
        goto init_vcpu_close;
    }
    INFO("VCPU mmap size is %dB", vcpu->mem_size);

    vcpu->kvm_run = mmap(NULL, vcpu->mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu->fd, 0);
    if (vcpu->kvm_run == MAP_FAILED) {
        ERROR("VCPU failed to allocate kvm shared region (%s)", strerror(errno));
        ret = -1;
        goto init_vcpu_close;
    }
    INFO("VCPU kvm shared memory region allocated succesfully at 0x%llx", (uint64_t)vcpu->kvm_run);

    return ret;

init_vcpu_close:
    close(vcpu->fd);
init_vcpu_exit:
    return ret;
}

int32_t init_memory(VirtualMachine *vm, const char *img_path) {
    int32_t ret = 0;
    int32_t img_fd = open(img_path, O_RDONLY);
    if (img_fd < 0) {
        ERROR("failed to open %s (%s)", img_path, strerror(errno));
        ret = -1;
        goto init_memory_exit;
    }
    INFO("guest %s opened", img_path);

    uint8_t *p = (uint8_t *)vm->mem;
    int32_t read_size = 0;
    while (true) {
        read_size = read(img_fd, p, 4096);

        if (read_size <= 0) {
            break;
        }

        TRACE("read %dB of %s", read_size, img_path);
        p += read_size;
    }

init_memory_exit:
    return ret;
}

int32_t setup_vcpu(VirtualCPU *vcpu) {
    int32_t ret = 0;

    if (ioctl(vcpu->fd, KVM_GET_REGS, &vcpu->regs) < 0) {
        ERROR("VCPU failed to get VCPU %d(%d) regs (%s)", vcpu->id, vcpu->fd, strerror(errno));
        ret = -1;
        goto setup_vcpu_ret;
    }
    INFO("VCPU %d registers copied", vcpu->id);

    memset(&vcpu->regs, 0, sizeof(struct kvm_regs));
    vcpu->regs.rflags = 0b10; // 2nd bit of rflags have to be set to 1
    vcpu->regs.rip = 0;
    vcpu->regs.rsp = MEM_SIZE >> 2;

    if (ioctl(vcpu->fd, KVM_SET_REGS, &vcpu->regs) < 0) {
        ERROR("VCPU failed to set registers (%s)", strerror(errno));
        ret = -1;
        goto setup_vcpu_ret;
    }
    INFO("VCPU new values written to registers");

    if (ioctl(vcpu->fd, KVM_GET_SREGS, &vcpu->sregs) < 0) {
        ERROR("VCPU failed to get VCPU %d(%d) sregs (%s)", vcpu->id, vcpu->fd, strerror(errno));
        ret = -1;
        goto setup_vcpu_ret;
    }
    INFO("VCPU %d special registers copied", vcpu->id);

    vcpu->sregs.cs.selector = 0;
    vcpu->sregs.cs.base = 0;
    vcpu->sregs.ds.selector = 0;
    vcpu->sregs.ds.base = 0;
    vcpu->sregs.ss.selector = 0;
    vcpu->sregs.ss.base = 0;

    if (ioctl(vcpu->fd, KVM_SET_SREGS, &vcpu->sregs) < 0) {
        ERROR("VCPU failed to set special registers (%s)", strerror(errno));
        ret = -1;
        goto setup_vcpu_ret;
    }
    INFO("VCPU new values written to special registers");

setup_vcpu_ret:
    return ret;
}

int32_t run_vm(VirtualCPU *vcpu) {
    int32_t ret = 0;
    int32_t exit_reason = 0;

    INFO("KVM start run");

    bool shutdown = false;
    while (!shutdown) {
        ret = ioctl(vcpu->fd, KVM_RUN, 0);
        if (ret < 0) {
            ERROR("KVM run failed with : %d (%s)", ret, strerror(errno));
            goto run_vm_exit;
        }

        exit_reason = vcpu->kvm_run->exit_reason;
        switch (exit_reason) {
        case KVM_EXIT_UNKNOWN:
            ERROR("KVM unknown exit reason, aborting");
            shutdown = true;
            break;

        case KVM_EXIT_DEBUG:
            WARN("KVM_EXIT_DEBUG");
            break;

        case KVM_EXIT_IO:
            INFO(
                "KVM_EXIT_IO port %s: %d, data: %d", vcpu->kvm_run->io.direction ? "out" : "in",
                vcpu->kvm_run->io.port,
                *(int32_t *)((char *)(vcpu->kvm_run) + vcpu->kvm_run->io.data_offset));

            sleep(1);
            break;

        case KVM_EXIT_HLT:
            TRACE("KVM vm is halting");
            break;

        case KVM_EXIT_MMIO:
            WARN("KVM_EXIT_MMIO");
            break;

        case KVM_EXIT_INTR:
            WARN("KVM_EXIT_INTR");
            break;

        case KVM_EXIT_SHUTDOWN:
            WARN("KVM_EXIT_SHUTDOWN");
            shutdown = true;
            break;

        case KVM_EXIT_FAIL_ENTRY:
            ERROR(
                "KVM KVM_RUN failed entry with: reason %u. cpu: %u",
                vcpu->kvm_run->fail_entry.hardware_entry_failure_reason,
                vcpu->kvm_run->fail_entry.cpu);
            shutdown = true;
            break;

        case KVM_EXIT_INTERNAL_ERROR:
            ERROR("KVM KVM_RUN internal error");
            shutdown = true;
            break;

        default:
            ERROR("KVM unhandled EXIT reason %d", exit_reason);
            shutdown = true;
            break;
        }

        // check for signals
        if (sig_status == SIGINT) {
            shutdown = true;
        }
    }

run_vm_exit:
    return ret;
}
