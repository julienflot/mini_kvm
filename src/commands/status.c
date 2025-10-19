#include "status.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "commands.h"
#include "constants.h"
#include "ipc/ipc.h"
#include "kvm/kvm.h"
#include "utils/errors.h"
#include "utils/logger.h"
#include "utils/utils.h"

typedef MiniKVMError (*CommandHandler)(Kvm *, MiniKvmStatusCommand *, MiniKvmStatusResult *);

static const int64_t MEM_RANGE_DEFAULTS[] = {0, -1, 2, 16};

static const struct option opts_def[] = {
    {"name", required_argument, NULL, 'n'}, {"vcpu", required_argument, NULL, 'v'},
    {"regs", no_argument, NULL, 'r'},       {"mem", required_argument, NULL, 'm'},
    {"help", no_argument, NULL, 'h'},       {0, 0, 0, 0}};

static void status_print_help() {
    printf("USAGE: mini_kvm run\n");
    printf("\t--name/-n: set the name of the virtual machine\n");
    printf("\t--regs/-r: request register state\n");
    printf("\t--vcpus/-v: specify a target VCPU list\n");
    printf(
        "\t--mem/-m: dump memory format is start_addr,[,end_addr][,word_size][,bytes_per_line]\n");
    printf("\t--help/-h: print this message\n");
}

static MiniKVMError status_parse_args(int argc, char **argv, MiniKvmStatusArgs *args) {
    MiniKVMError ret = MINI_KVM_SUCCESS;
    int32_t index = 0, name_len = 0;
    char c = 0;

    while (c != -1 && ret != MINI_KVM_ARGS_FAILED) {
        c = getopt_long(argc, argv, "n:v:rm:h", opts_def, &index);

        switch (c) {
        case 'n':
            name_len = strlen(optarg);
            args->name = malloc(sizeof(char) * (name_len + 1));
            strncpy(args->name, optarg, name_len + 1);
            break;
        case 'r':
            args->regs = true;
            args->cmds[args->cmd_count] = MINI_KVM_COMMAND_SHOW_REGS;
            args->cmd_count += 1;
            break;
        case 'v':
            if (mini_kvm_parse_cpu_list((char *)optarg, &args->vcpus) != MINI_KVM_SUCCESS) {
                ERROR("invalid cpu list %s", optarg);
                ret = MINI_KVM_ARGS_FAILED;
            }
            break;
        case 'm':
            if (mini_kvm_parse_int_list((char *)optarg, &args->mem_range, &args->mem_range_size)) {
                ERROR("invalid mem range format %s", optarg);
                ret = MINI_KVM_ARGS_FAILED;
            }
            args->cmds[args->cmd_count] = MINI_KVM_COMMAND_DUMP_MEM;
            args->cmd_count += 1;
            break;
        case 'h':
        case '?':
            ret = MINI_KVM_ARGS_FAILED;
            break;
        }
    }

    // if no vcpu list was given, mini_kvm select all vcpus
    if (args->vcpus == 0 && args->regs) {
        args->vcpus = !0;
    }

    // if no other command has been specified, fallback to show state command
    if (args->cmd_count == 0) {
        args->cmds[args->cmd_count] = MINI_KVM_COMMAND_SHOW_STATE;
        args->cmd_count += 1;
    }

    return ret;
}

static MiniKVMError status_build_command(MiniKvmStatusArgs *args, uint32_t cmd_index,
                                         MiniKvmStatusCommand *cmd) {
    MiniKvmStatusCommandType type = args->cmds[cmd_index];

    switch (type) {
    case MINI_KVM_COMMAND_SHOW_STATE:
        cmd->type = type;
        break;
    case MINI_KVM_COMMAND_SHOW_REGS:
        cmd->type = type;
        cmd->vcpus = args->vcpus;
        break;
    case MINI_KVM_COMMAND_DUMP_MEM:
        cmd->type = type;
        for (uint32_t i = 0; i < args->mem_range_size; i++) {
            cmd->mem_range[i] = args->mem_range[i];
        }
        for (uint32_t i = args->mem_range_size; i < 4; i++) {
            cmd->mem_range[i] = MEM_RANGE_DEFAULTS[i];
        }
        cmd->pid = getpid();
        break;
    default:
        break;
    }

    return MINI_KVM_SUCCESS;
}

static MiniKVMError status_send_command(MiniKvmStatusArgs *args, int32_t sock, uint32_t cmd_index,
                                        MiniKvmStatusResult *res) {
    int32_t ret = MINI_KVM_SUCCESS;
    MiniKvmStatusCommand cmd = {0};

    if (status_build_command(args, cmd_index, &cmd) != MINI_KVM_SUCCESS) {
        ret = MINI_KVM_STATUS_COMMAND_FAILED;
        goto out;
    }

    if (mini_kvm_ipc_send_cmd(sock, &cmd, res)) {
        ret = MINI_KVM_STATUS_COMMAND_FAILED;
        goto out;
    }

out:
    return ret;
}

void status_handle_command_result(MiniKvmStatusArgs *args, MiniKvmStatusResult *res) {
    if (res->error != MINI_KVM_SUCCESS) {
        switch (res->error) {
        case MINI_KVM_STATUS_CMD_VM_NOT_PAUSED:
            printf("VM %s is not paused, please pause the VM before sending request\n", args->name);
            break;
        default:
            break;
        }

        return;
    }

    switch (res->cmd_type) {
    case MINI_KVM_COMMAND_NONE:
        break;
    case MINI_KVM_COMMAND_SHOW_STATE:
        printf("%s state: %s\n", args->name, mini_kvm_vm_state_str(res->state));
        break;
    case MINI_KVM_COMMAND_SHOW_REGS:
        for (uint64_t index = 0; index < MINI_KVM_MAX_VCPUS; index++) {
            if ((res->vcpus & (1UL << index)) == 0) {
                continue;
            }

            printf("VCPU %lu regs\n", index);
            mini_kvm_print_regs(&res->regs[index]);
            printf("\n");
            printf("VCPU %lu sregs\n", index);
            mini_kvm_print_sregs(&res->sregs[index]);
        }
        break;
    default:
        break;
    }
}

MiniKVMError mini_kvm_status(int argc, char **argv) {
    MiniKVMError ret = MINI_KVM_SUCCESS;
    MiniKvmStatusArgs args = {0};
    MiniKvmStatusResult res = {0};
    struct sockaddr_un addr = {0};
    int32_t sock = 0;

    ret = status_parse_args(argc, argv, &args);
    if (ret != 0) {
        status_print_help();
        goto clean;
    }

    if (args.name == NULL) {
        printf("no VM name was specified, exiting ...\n");
        goto clean;
    }

    if (mini_kvm_check_vm(args.name) < 0) {
        printf("VM %s is not running, exiting ...\n", args.name);
        goto clean;
    }

    if ((sock = mini_kvm_ipc_connect(args.name, &addr)) < 0) {
        ERROR("failed to connect to socket %s", args.name);
        ret = MINI_KVM_INTERNAL_ERROR;
        goto clean;
    }
    for (uint32_t i = 0; i < args.cmd_count; i++) {
        ret = status_send_command(&args, sock, i, &res);
        if (ret != 0) {
            goto close_socket;
        }

        status_handle_command_result(&args, &res);
    }

close_socket:
    close(sock);
clean:
    if (args.name != NULL) {
        free(args.name);
    }

    if (args.mem_range) {
        free(args.mem_range);
    }

    return ret;
}

static MiniKVMError status_handle_cmd_state(Kvm *kvm,
                                            __attribute__((unused)) MiniKvmStatusCommand *cmd,
                                            MiniKvmStatusResult *res) {
    res->state = kvm->state;
    return MINI_KVM_SUCCESS;
}

static MiniKVMError status_handle_regs(Kvm *kvm, MiniKvmStatusCommand *cmd,
                                       MiniKvmStatusResult *res) {
    if (kvm->state != MINI_KVM_PAUSED) {
        return MINI_KVM_STATUS_CMD_VM_NOT_PAUSED;
    }

    for (uint64_t index = 0; index < kvm->vcpu_count; index++) {
        if (!(cmd->vcpus & (1UL << index))) {
            continue;
        }

        VCpu vcpu = kvm->vcpus[index];
        if (ioctl(vcpu.fd, KVM_GET_REGS, &res->regs)) {
            ERROR("failed to vcpu %u registers (%s)", index, strerror(errno));
            return MINI_KVM_INTERNAL_ERROR;
        }
        if (ioctl(vcpu.fd, KVM_GET_SREGS, &res->sregs)) {
            ERROR("failed to vcpu %u sregisters (%s)", index, strerror(errno));
            return MINI_KVM_INTERNAL_ERROR;
        }
    }

    return MINI_KVM_SUCCESS;
}

MiniKVMError status_handle_dump_mem(Kvm *kvm, MiniKvmStatusCommand *cmd,
                                    __attribute__((unused)) MiniKvmStatusResult *res) {
    int remote_stdoutfd;
    char remote_stdoutpath[64];

    if (kvm->state != MINI_KVM_PAUSED) {
        return MINI_KVM_STATUS_CMD_VM_NOT_PAUSED;
    }

    snprintf(remote_stdoutpath, sizeof(remote_stdoutpath), "/proc/%d/fd/1", cmd->pid);
    remote_stdoutfd = open(remote_stdoutpath, O_RDWR);
    if (remote_stdoutfd != -1) {
        mini_kvm_dump_mem(kvm, remote_stdoutfd, cmd->mem_range[0],
                          (cmd->mem_range[1] == -1) ? kvm->mem_size : cmd->mem_range[1],
                          cmd->mem_range[2], cmd->mem_range[3]);
        close(remote_stdoutfd);
    } else {
        ERROR("failed to serve DUMP MEM command (%s)", strerror(errno));
        return MINI_KVM_INTERNAL_ERROR;
    }

    return MINI_KVM_SUCCESS;
}

static MiniKVMError status_handle_pause(Kvm *kvm, __attribute__((unused)) MiniKvmStatusCommand *cmd,
                                        __attribute((unused)) MiniKvmStatusResult *res) {
    kvm->state = MINI_KVM_PAUSED;
    mini_kvm_send_sig(kvm, SIGVMPAUSE);

    return MINI_KVM_SUCCESS;
}

static MiniKVMError status_handle_resume(Kvm *kvm,
                                         __attribute__((unused)) MiniKvmStatusCommand *cmd,
                                         __attribute((unused)) MiniKvmStatusResult *res) {
    kvm->state = MINI_KVM_RUNNING;
    mini_kvm_send_sig(kvm, SIGVMRESUME);

    return MINI_KVM_SUCCESS;
}

static MiniKVMError status_handle_shutdown(Kvm *kvm,
                                           __attribute__((unused)) MiniKvmStatusCommand *cmd,
                                           __attribute((unused)) MiniKvmStatusResult *res) {
    kvm->state = MINI_KVM_SHUTDOWN;
    mini_kvm_send_sig(kvm, SIGVMSHUTDOWN);

    return MINI_KVM_SUCCESS;
}

static MiniKVMError status_handle_none(__attribute__((unused)) Kvm *kvm,
                                       __attribute__((unused)) MiniKvmStatusCommand *cmd,
                                       __attribute((unused)) MiniKvmStatusResult *res) {
    return MINI_KVM_SUCCESS;
}

MiniKVMError mini_kvm_status_handle_command(Kvm *kvm, MiniKvmStatusCommand *cmd,
                                            MiniKvmStatusResult *res) {
    static const CommandHandler handlers[MINI_KVM_COMMAND_COUNT] = {
        [MINI_KVM_COMMAND_NONE] = status_handle_none,
        [MINI_KVM_COMMAND_PAUSE] = status_handle_pause,
        [MINI_KVM_COMMAND_RESUME] = status_handle_resume,
        [MINI_KVM_COMMAND_SHUTDOWN] = status_handle_shutdown,
        [MINI_KVM_COMMAND_SHOW_STATE] = status_handle_cmd_state,
        [MINI_KVM_COMMAND_SHOW_REGS] = status_handle_regs,
        [MINI_KVM_COMMAND_DUMP_MEM] = status_handle_dump_mem,
    };
    MiniKVMError ret = MINI_KVM_SUCCESS;

    pthread_mutex_lock(&kvm->lock);
    ret = handlers[cmd->type](kvm, cmd, res);
    res->cmd_type = cmd->type;
    res->vcpus = cmd->vcpus;
    res->error = ret;
    pthread_mutex_unlock(&kvm->lock);

    return ret;
}
