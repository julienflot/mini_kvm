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
#include <sys/ptrace.h>
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

static socklen_t SOCKET_SIZE = sizeof(struct sockaddr_un);
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
            status_print_help();
            ret = 1;
            break;
        case '?':
            ret = MINI_KVM_ARGS_FAILED;
            break;
        }
    }

    // if no vcpu list was given, mini_kvm select all vcpus
    if (args->vcpus == 0 && args->regs) {
        args->vcpus = !0;
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
    switch (res->cmd_type) {
    case MINI_KVM_COMMAND_NONE:
        break;
    case MINI_KVM_COMMAND_SHOW_STATE:
        TRACE("%s is %s", args->name, mini_kvm_vm_state_str(res->state));
        break;
    case MINI_KVM_COMMAND_SHOW_REGS:
        for (uint64_t index = 0; index < MINI_KVM_MAX_VCPUS; index++) {
            if ((res->vcpus & (1UL << index)) == 0) {
                continue;
            }

            TRACE("VCPU %u regs", index);
            mini_kvm_print_regs(&res->regs[index]);
            TRACE("VCPU %u sregs", index);
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
        INFO("status: no name was specified, exiting ...");
        goto clean;
    }

    if (mini_kvm_check_vm(args.name) < 0) {
        INFO("status: VM %s is not running, exiting ...", args.name);
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

MiniKVMError mini_kvm_status_create_socket(Kvm *kvm, struct sockaddr_un *addr) {
    MiniKVMError ret = MINI_KVM_SUCCESS;

    addr->sun_family = AF_UNIX;
    sprintf(addr->sun_path, "%s/%s.sock", kvm->fs_path, kvm->name);

    kvm->sock = socket(addr->sun_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (kvm->sock < 0) {
        ERROR("unable to create status socket (%s)", strerror(errno));
        ret = MINI_KVM_FAILED_SOCKET_CREATION;
        goto out;
    }

    if (bind(kvm->sock, (struct sockaddr *)addr, SOCKET_SIZE) < 0) {
        ERROR("unable to bind to socket (%s)", strerror(errno));
        ret = MINI_KVM_FAILED_SOCKET_CREATION;
        goto close_socket;
    }

    if (listen(kvm->sock, 0) < 0) {
        ERROR("unable to listen to socket (%s)", strerror(errno));
        ret = MINI_KVM_FAILED_SOCKET_CREATION;
        goto close_socket;
    }

    goto out;

close_socket:
    close(kvm->sock);
out:
    return ret;
}

int32_t mini_kvm_status_receive_cmd(Kvm *kvm) {
    struct sockaddr_un remote_addr = {0};
    int32_t remote_sock = 0;

    remote_sock = accept(kvm->sock, (struct sockaddr *)&remote_addr, &SOCKET_SIZE);
    if (remote_sock < 0 && errno == EAGAIN) {
        return 0;
    }

    if (remote_sock < 0) {
        ERROR("unable to accept connection (%s)", strerror(errno));
        return -1;
    }

    return remote_sock;
}

MiniKVMError mini_kvm_status_handle_command(Kvm *kvm, MiniKvmStatusCommand *cmd,
                                            MiniKvmStatusResult *res) {
    MiniKVMError ret = MINI_KVM_SUCCESS;
    int remote_stdoutfd;
    char remote_stdoutpath[64];

    pthread_mutex_lock(&kvm->lock);
    switch (cmd->type) {
    case MINI_KVM_COMMAND_SHOW_STATE:
        res->state = kvm->state;
        break;
    case MINI_KVM_COMMAND_SHOW_REGS:
        for (uint64_t index = 0; index < kvm->vcpu_count; index++) {
            if (!(cmd->vcpus & (1UL << index))) {
                continue;
            }

            VCpu vcpu = kvm->vcpus[index];
            if (ioctl(vcpu.fd, KVM_GET_REGS, &res->regs)) {
                ERROR("failed to vcpu %u registers (%s)", index, strerror(errno));
                ret = MINI_KVM_INTERNAL_ERROR;
                goto out;
            }
            if (ioctl(vcpu.fd, KVM_GET_SREGS, &res->sregs)) {
                ERROR("failed to vcpu %u sregisters (%s)", index, strerror(errno));
                ret = MINI_KVM_INTERNAL_ERROR;
                goto out;
            }
        }
        break;
    case MINI_KVM_COMMAND_DUMP_MEM:
        snprintf(remote_stdoutpath, sizeof(remote_stdoutpath), "/proc/%d/fd/1", cmd->pid);
        remote_stdoutfd = open(remote_stdoutpath, O_RDWR);
        if (remote_stdoutfd != -1) {
            mini_kvm_dump_mem(kvm, remote_stdoutfd, cmd->mem_range[0],
                              (cmd->mem_range[1] == -1) ? kvm->mem_size : cmd->mem_range[1],
                              cmd->mem_range[2], cmd->mem_range[3]);
            close(remote_stdoutfd);
        } else {
            ERROR("failed to serve DUMP MEM command (%s)", strerror(errno));
        }
        break;
    case MINI_KVM_COMMAND_PAUSE:
        kvm->state = MINI_KVM_PAUSED;
        mini_kvm_send_sig(kvm, SIGVMPAUSE);
        break;
    case MINI_KVM_COMMAND_RESUME:
        kvm->state = MINI_KVM_RUNNING;
        mini_kvm_send_sig(kvm, SIGVMPAUSE);
        break;
    case MINI_KVM_COMMAND_SHUTDOWN:
        kvm->state = MINI_KVM_SHUTDOWN;
        mini_kvm_send_sig(kvm, SIGVMSHUTDOWN);
        break;
    case MINI_KVM_COMMAND_NONE: // do nothing
    case MINI_KVM_COMMAND_COUNT:
        break;
    }
    res->cmd_type = cmd->type;
    res->vcpus = cmd->vcpus;

out:
    pthread_mutex_unlock(&kvm->lock);
    return ret;
}
