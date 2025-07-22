#include "run.h"
#include "kvm/kvm.h"
#include "utils/errors.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct MiniKvmRunArgs {
    bool log_enabled;
    uint32_t vcpu;
    uint32_t mem_size;
    uint64_t kernel_size;
    uint8_t *kernel_code;
} MiniKvmRunArgs;

static const struct option opts_def[] = {
    {"log", optional_argument, NULL, 'l'},
    {"help", no_argument, NULL, 'h'},
    {"vcpu", required_argument, NULL, 'v'},
    {"disk", required_argument, NULL, 'd'},
    {"mem", required_argument, NULL, 'm'},
    {"kernel", required_argument, NULL, 'k'},
    {0, 0, 0, 0}};

static int32_t parse_mem(char *arg, uint32_t *mem) {
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
    printf("\t--log/-l: enable logging, can specify an output file with --log=output.txt\n");
    printf("\t--mem/-m: memory allocated to the virtual machine in bytes\n");
    printf("\t--vcpu/-v: number of vcpus dedicated to the virtual machine\n");
    printf("\t--help/-h: print this message\n");
}

static int run_parser_args(int argc, char **argv, MiniKvmRunArgs *args) {
    int ret = 0;
    int index = 0;
    char c = 0;
    FILE *kernel_file = NULL;
    while (c != -1 && ret != MINI_KVM_ARGS_FAILED) {
        c = getopt_long(argc, argv, "l::v::d:m:h", opts_def, &index);

        // TODO: enable support for disk option
        switch (c) {
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
            to_number(optarg, strlen(optarg), &args->vcpu);
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

int mini_kvm_run(int argc, char **argv) {
    int ret = 0;
    Kvm *kvm = NULL;
    MiniKvmRunArgs args = {0};

    ret = run_parser_args(argc, argv, &args);
    if (ret != 0) {
        goto out;
    }
    INFO("mini_kvm: argument parsing successful, starts initialization");

    kvm = malloc(sizeof(Kvm));
    if (kvm == NULL) {
        ret = MINI_KVM_FAILED_ALLOCATION;
        ERROR("failed to allocate mini kvm struct");
        goto out;
    }

    ret = mini_kvm_setup_kvm(kvm, args.mem_size);
    if (ret != 0) {
        goto clean_kvm_struct;
    }

    ret = load_kernel(kvm, &args, 0);
    if (ret != 0) {
        goto clean_kvm_struct;
    }
    INFO("kernel loaded in guest memory");

    for (uint32_t i = 0; i < args.vcpu; i++) {
        ret = mini_kvm_add_vcpu(kvm);
        if (ret != MINI_KVM_SUCCESS) {
            goto clean_kvm_struct;
        }

        ret = mini_kvm_setup_vcpu(kvm, i);
        if (ret != MINI_KVM_SUCCESS) {
            goto clean_kvm_struct;
        }
    }

    mini_kvm_vcpu_run(kvm, 0);

clean_kvm_struct:
    mini_kvm_clean_kvm(kvm);

    if (args.kernel_code != NULL) {
        free(args.kernel_code);
    }

out:
    return ret;
}
