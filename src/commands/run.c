#include "run.h"
#include "commands.h"
#include "commands/status.h"
#include "constants.h"
#include "ipc/ipc.h"
#include "kvm/kvm.h"
#include "utils/errors.h"
#include "utils/filesystem.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static volatile sig_atomic_t sig_status = 0;
static void set_signal_status(int signo) { sig_status = signo; }

static const struct option opts_def[] = {
    {"name", required_argument, NULL, 'n'},   {"log", optional_argument, NULL, 'l'},
    {"help", no_argument, NULL, 'h'},         {"vcpu", required_argument, NULL, 'v'},
    {"disk", required_argument, NULL, 'd'},   {"mem", required_argument, NULL, 'm'},
    {"kernel", required_argument, NULL, 'k'}, {0, 0, 0, 0}};

static int32_t parse_mem(char *arg, uint64_t *mem) {
    uint32_t arg_len = strlen(arg);
    if (arg_len == 0) {
        return -1;
    }

    // TODO: find a better scale for units
    uint32_t unit_scale = 1;
    switch (arg[arg_len - 1]) {
    case 'K':
        unit_scale = 1000;
        break;
    case 'M':
        unit_scale = 1000000;
        break;
    case 'G':
        unit_scale = 1000000000;
        break;
    default:
        if (!mini_kvm_is_uint(arg + arg_len - 1, 1)) {
            TRACE("run:parse_mem unknown unit %s", arg + arg_len - 1);
            return MINI_KVM_ARGS_FAILED;
        }
    }

    // if not unit was provided, we do not need to remove the last char
    uint32_t offset = (unit_scale != 1) ? 1 : 0;
    if (!mini_kvm_is_uint(arg, arg_len - offset)) {
        return -1;
    }
    mini_kvm_to_uint(arg, arg_len - offset, mem);
    *mem *= unit_scale;

    return MINI_KVM_SUCCESS;
}

void run_print_help() {
    printf("USAGE: mini_kvm run\n");
    printf("\t--name/-n: set the name of the virtual machine\n");
    printf("\t--log/-l: enable logging, can specify an output file with --log=output.txt\n");
    printf("\t--mem/-m: memory allocated to the virtual machine in bytes\n");
    printf("\t--vcpu/-v: number of vcpus dedicated to the virtual machine\n");
    printf("\t--help/-h: print this message\n");
}

MiniKVMError run_parse_args(int argc, char **argv, MiniKvmRunArgs *args) {
    MiniKVMError ret = 0;
    int32_t index = 0;
    char c = 0;
    FILE *kernel_file = NULL;
    uint32_t name_len = 0;

    while (c != -1 && ret != MINI_KVM_ARGS_FAILED) {
        c = getopt_long(argc, argv, "l::v:d:m:n:k:h", opts_def, &index);

        // TODO: enable support for disk option
        switch (c) {
        case 'n':
            name_len = strlen(optarg);
            args->name = malloc(sizeof(char) * (name_len + 1));
            strncpy(args->name, optarg, name_len + 1);
            break;
        case 'l':
            args->log_enabled = true;
            if (optarg) {
                logger_set_output(optarg);
            }
            break;

        case 'm':
            ret = parse_mem(optarg, &args->mem_size);
            if (ret != 0) {
                ERROR("failed to parse mem argument : %s", optarg);
            }
            break;

        case 'v':
            if (!mini_kvm_is_uint(optarg, strlen(optarg))) {
                ERROR("--vcpu expect a digit, got : %s", optarg);
                ret = MINI_KVM_ARGS_FAILED;
            }
            mini_kvm_to_uint(optarg, strlen(optarg), (uint64_t *)&args->vcpu);
            break;

        case 'k':
            kernel_file = fopen(optarg, "rb");
            if (kernel_file == NULL) {
                ERROR("unable to open kernel code (%s)", strerror(errno));
                ret = MINI_KVM_ARGS_FAILED;
            }

            // trick to get the size in bytes of the file (needed for allocating correct mem size)
            fseek(kernel_file, 0, SEEK_END);
            args->kernel_size = ftell(kernel_file);
            fseek(kernel_file, 0, SEEK_SET);

            args->kernel_code = malloc(sizeof(uint8_t) * args->kernel_size);
            fread(args->kernel_code, 1, args->kernel_size, kernel_file);
            fclose(kernel_file);

            break;

        case 'h':
        case '?':
            run_print_help();
            ret = MINI_KVM_ARGS_FAILED;
            break;
        }
    }

    return ret;
}

static MiniKVMError load_kernel(Kvm *kvm, MiniKvmRunArgs *args, uint64_t addr) {
    if (kvm == NULL || args == NULL) {
        ERROR("kvm or args are initialized, unable to load kernel in guest memory");
        return MINI_KVM_INTERNAL_ERROR;
    }

    if (args->kernel_code == NULL || args->kernel_size == 0) {
        ERROR("kernel code is empty");
        return MINI_KVM_INTERNAL_ERROR;
    }

    if (kvm->mem == NULL) {
        ERROR("guest memory is unitialized");
        return MINI_KVM_INTERNAL_ERROR;
    }

    uint64_t *start = (uint64_t *)((uint8_t *)kvm->mem + addr);
    memcpy(start, args->kernel_code, args->kernel_size);

    return MINI_KVM_SUCCESS;
}

static MiniKVMError init_filesystem(char *name, Kvm *kvm) {
    MiniKVMError ret = MINI_KVM_SUCCESS;
    int32_t root_dir_fd = 0, fs_exists = -1, pid_file = 0, pid = 0;
    char *pidfile_name = NULL;
    struct stat dir_stat = {};

    if (stat(MINI_KVM_FS_ROOT_PATH, &dir_stat) == -1) {
        if (mkdir(MINI_KVM_FS_ROOT_PATH, 0700) < -1) {
            ERROR("failed to create %s (%s)", MINI_KVM_FS_ROOT_PATH, strerror(errno));
            return MINI_KVM_FAILED_FS_SETUP;
        }
    }

    kvm->fs_path = malloc(sizeof(char) * (strlen(name) + strlen(MINI_KVM_FS_ROOT_PATH) + 2));
    sprintf(kvm->fs_path, "%s/%s", MINI_KVM_FS_ROOT_PATH, name);

    fs_exists = stat(kvm->fs_path, &dir_stat);
    root_dir_fd = open(MINI_KVM_FS_ROOT_PATH, O_DIRECTORY | O_RDONLY);
    mkdirat(root_dir_fd, name, 0700);
    kvm->fs_fd = openat(root_dir_fd, name, O_DIRECTORY | O_RDONLY);
    if (kvm->fs_fd < 0) {
        ERROR("failed to create %s (%s)", kvm->fs_path, strerror(errno));
        ret = MINI_KVM_FAILED_FS_SETUP;
        goto close_main_dir;
    }

    pidfile_name = malloc(sizeof(char) * (strlen(name) + 4 + 1));
    sprintf(pidfile_name, "%s.pid", name);

    if (fs_exists == 0) {
        pid_file = openat(kvm->fs_fd, pidfile_name, O_RDONLY);
        if (pid_file > 0) {
            read(pid_file, &pid, sizeof(int));
            if (kill(pid, 0) == 0) {
                ERROR("a virtual machine with the same name is already running");
                ret = MINI_KVM_FAILED_FS_SETUP;
                close(pid_file);
                goto clean;
            } else {
                close(pid_file);
            }
        }
    }

    pid_file = openat(kvm->fs_fd, pidfile_name, O_CREAT | O_TRUNC | O_WRONLY,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (pid_file < 0) {
        ERROR("failed to create %s pidfile (%s)", name, strerror(errno));
        ret = MINI_KVM_FAILED_FS_SETUP;
        goto clean;
    }
    pid = getpid();
    write(pid_file, &pid, sizeof(int));

    close(pid_file);
clean:
    free(pidfile_name);
close_main_dir:
    close(root_dir_fd);

    return ret;
}

static void run_set_signals() {
    if (signal(SIGINT, set_signal_status) == SIG_ERR) {
        WARN("unable to register to signal SIGINT");
    }

    if (signal(SIGTERM, set_signal_status) == SIG_ERR) {
        WARN("unable to register to signal SIGINT");
    }
}

static MiniKVMError run_main_loop(Kvm *kvm) {
    MiniKVMError ret = MINI_KVM_SUCCESS;
    int32_t remote_sock = 0;
    struct sockaddr_un socket_addr = {0};
    MiniKvmStatusCommand cmd = {0};
    MiniKvmStatusResult res = {0};

    // create main ipc socket
    ret = mini_kvm_ipc_create_socket(kvm, &socket_addr);
    if (ret != MINI_KVM_SUCCESS) {
        goto out;
    }

    // start vm
    ret = mini_kvm_start_vm(kvm);
    if (ret != MINI_KVM_SUCCESS) {
        goto out;
    }

    while (kvm->state != MINI_KVM_SHUTDOWN) {
        remote_sock = mini_kvm_ipc_receive_cmd(kvm);
        if (remote_sock > 0) {
            // commands are handled until the socket is closed by the remote
            while (recv(remote_sock, &cmd, sizeof(MiniKvmStatusCommand), 0) > 0) {
                mini_kvm_status_handle_command(kvm, &cmd, &res);
                send(remote_sock, &res, sizeof(res), 0);
            }
        } else if (remote_sock < 0) {
            WARN("unable to receive command");
        }

        if (sig_status == SIGINT || sig_status == SIGTERM) {
            kvm->state = MINI_KVM_SHUTDOWN;
            mini_kvm_send_sig(kvm, SIGVMSHUTDOWN);
        }

        usleep(100000);
    }

out:
    return ret;
}

MiniKVMError mini_kvm_run(int argc, char **argv) {
    MiniKVMError ret = 0;
    Kvm *kvm = NULL;
    MiniKvmRunArgs args = {0};

    ret = run_parse_args(argc, argv, &args);
    if (ret != 0) {
        goto out;
    }
    INFO("mini_kvm: argument parsing successful, starts initialization");

    kvm = calloc(1, sizeof(Kvm));
    if (kvm == NULL) {
        ret = MINI_KVM_FAILED_ALLOCATION;
        ERROR("failed to allocate mini kvm struct");
        goto out;
    }

    ret = mini_kvm_setup_kvm(kvm, args.mem_size);
    if (ret != 0) {
        goto clean_kvm;
    }

    // if vcpu number has not been specified by the user, mini_kvm set it to at least one
    if (args.vcpu == 0) {
        args.vcpu = 1;
    }
    for (uint32_t i = 0; i < args.vcpu; i++) {
        ret = mini_kvm_add_vcpu(kvm);
        if (ret != MINI_KVM_SUCCESS) {
            goto clean_kvm;
        }

        ret = mini_kvm_setup_vcpu(kvm, i, 4096);
        if (ret != MINI_KVM_SUCCESS) {
            goto clean_fs;
        }
    }

    ret = load_kernel(kvm, &args, 4096);
    if (ret != 0) {
        goto clean_kvm;
    }
    INFO("kernel loaded in guest memory");

    if (args.name != NULL && args.name[0] != '\0') {
        kvm->name = malloc(sizeof(char) * (strlen(args.name) + 1));
        strncpy(kvm->name, args.name, strlen(args.name) + 1);
        if (init_filesystem(args.name, kvm)) {
            goto clean_kernel;
        }
        INFO("filesystem initialized for VM %s", args.name);
    }

    run_set_signals();

    run_main_loop(kvm);

clean_fs:
    if (args.name != NULL) {
        rmrf(kvm->fs_path);
    }

clean_kernel:
    if (args.kernel_code != NULL) {
        free(args.kernel_code);
    }

clean_kvm:
    mini_kvm_clean_kvm(kvm);
out:
    return ret;
}
