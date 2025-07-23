#include "run.h"
#include "constants.h"
#include "kvm/kvm.h"
#include "utils/errors.h"
#include "utils/filesystem.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
    }

    // if not unit was provided, we do not need to remove the last char
    uint32_t offset = (unit_scale != 1) ? 1 : 0;
    if (!is_number(arg, arg_len - offset)) {
        return -1;
    }
    to_number(arg, arg_len - offset, mem);
    *mem *= unit_scale;

    return 0;
}

void run_print_help() {
    printf("USAGE: mini_kvm run\n");
    printf("\t--name/-n: set the name of the virtual machine\n");
    printf("\t--log/-l: enable logging, can specify an output file with --log=output.txt\n");
    printf("\t--mem/-m: memory allocated to the virtual machine in bytes\n");
    printf("\t--vcpu/-v: number of vcpus dedicated to the virtual machine\n");
    printf("\t--help/-h: print this message\n");
}

int run_parse_args(int argc, char **argv, MiniKvmRunArgs *args) {
    int ret = 0, index = 0;
    char c = 0;
    FILE *kernel_file = NULL;
    uint32_t name_len = 0;

    while (c != -1 && ret != MINI_KVM_ARGS_FAILED) {
        c = getopt_long(argc, argv, "l::v:d:m:n:h", opts_def, &index);

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
            if (ret < 0) {
                ERROR("failed to parse mem argument : %s", optarg);
            }
            break;

        case 'v':
            if (!is_number(optarg, strlen(optarg))) {
                ERROR("--vcpu expect a digit, got : %s", optarg);
                ret = MINI_KVM_ARGS_FAILED;
            }
            to_number(optarg, strlen(optarg), (uint64_t *)&args->vcpu);
            break;

        case 'k':
            kernel_file = fopen(optarg, "rb");
            if (kernel_file == NULL) {
                ERROR("unable to open kernel code (%s)", strerror(errno));
                ret = MINI_KVM_ARGS_FAILED;
            }
            fseek(kernel_file, 0, SEEK_END);
            args->kernel_size = ftell(kernel_file);
            fseek(kernel_file, 0, SEEK_SET);

            args->kernel_code = malloc(sizeof(uint8_t) * args->kernel_size);
            fread(args->kernel_code, 1, args->kernel_size, kernel_file);
            fclose(kernel_file);

            break;

        case 'h':
            run_print_help();
            ret = 1;
            break;

        case '?':
            run_print_help();
            ret = MINI_KVM_ARGS_FAILED;
            break;
        }
    }

    return ret;
}

static int32_t load_kernel(Kvm *kvm, MiniKvmRunArgs *args, uint64_t addr) {
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

    uint64_t *start = kvm->mem + addr;
    memcpy(start, args->kernel_code, args->kernel_size);

    return MINI_KVM_SUCCESS;
}

int32_t init_filesystem(char *name) {
    struct stat root_dir_stat = {};
    if (stat(MINI_KVM_FS_ROOT_PATH, &root_dir_stat) == -1) {
        if (mkdir(MINI_KVM_FS_ROOT_PATH, 0700) < -1) {
            ERROR("failed to create %s (%s)", MINI_KVM_FS_ROOT_PATH, strerror(errno));
            return MINI_KVM_FAILED_FS_SETUP;
        }
    }

    int root_dir_fs = open(MINI_KVM_FS_ROOT_PATH, O_DIRECTORY | O_RDONLY);
    mkdirat(root_dir_fs, name, 0700);
    int vm_dir_fs = openat(root_dir_fs, name, O_DIRECTORY | O_RDONLY);

    char *pidfile_name = malloc(sizeof(char) * (strlen(name) + 4 + 1));
    sprintf(pidfile_name, "%s.pid", name);
    int vm_pid_file = openat(
        vm_dir_fs, pidfile_name, O_CREAT | O_RDWR | O_TRUNC,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (vm_pid_file < -1) {
        ERROR("failed to create %s pidfile (%s)", name, strerror(errno));
        return MINI_KVM_FAILED_FS_SETUP;
    }

    int mainpid = getpid();
    write(vm_pid_file, &mainpid, sizeof(int));

    close(vm_pid_file);
    close(vm_dir_fs);
    close(root_dir_fs);
    free(pidfile_name);

    TRACE("filesystem initialized for VM %s", name);
    return MINI_KVM_SUCCESS;
}

int mini_kvm_run(int argc, char **argv) {
    int ret = 0;
    Kvm *kvm = NULL;
    MiniKvmRunArgs args = {0};

    ret = run_parse_args(argc, argv, &args);
    if (ret != 0) {
        goto out;
    }
    INFO("mini_kvm: argument parsing successful, starts initialization");

    if (args.name != NULL && args.name[0] != '\0') {
        init_filesystem(args.name);
    }

    kvm = calloc(1, sizeof(Kvm));
    if (kvm == NULL) {
        ret = MINI_KVM_FAILED_ALLOCATION;
        ERROR("failed to allocate mini kvm struct");
        goto out;
    }

    ret = mini_kvm_setup_kvm(kvm, args.mem_size);
    if (ret != 0) {
        goto clean;
    }

    ret = load_kernel(kvm, &args, 0);
    if (ret != 0) {
        goto clean;
    }
    INFO("kernel loaded in guest memory");

    // if vcpu number has not been specified by the user, mini_kvm set it to at least one
    if (args.vcpu == 0) {
        args.vcpu = 1;
    }
    for (uint32_t i = 0; i < args.vcpu; i++) {
        ret = mini_kvm_add_vcpu(kvm);
        if (ret != MINI_KVM_SUCCESS) {
            goto clean;
        }

        ret = mini_kvm_setup_vcpu(kvm, i);
        if (ret != MINI_KVM_SUCCESS) {
            goto clean;
        }
    }

    mini_kvm_vcpu_run(kvm, 0);

clean:
    mini_kvm_clean_kvm(kvm);

    if (args.kernel_code != NULL) {
        free(args.kernel_code);
    }

    if (args.name != NULL) {
        char *path = malloc(sizeof(char) * (strlen(args.name) + strlen(MINI_KVM_FS_ROOT_PATH) + 2));
        sprintf(path, "%s/%s", MINI_KVM_FS_ROOT_PATH, args.name);
        rmrf(path);
        free(path);
        free(args.name);
    }

out:
    return ret;
}
