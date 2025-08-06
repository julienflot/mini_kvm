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

static socklen_t SOCKET_SIZE = sizeof(struct sockaddr_un);

static const struct option opts_def[] = {{"name", required_argument, NULL, 'n'},
                                         {"vcpu", required_argument, NULL, 'v'},
                                         {"regs", no_argument, NULL, 'r'},
                                         {"help", no_argument, NULL, 'h'},
                                         {0, 0, 0, 0}};

static void status_print_help() {
    printf("USAGE: mini_kvm run\n");
    printf("\t--name/-n: set the name of the virtual machine\n");
    printf("\t--regs/-r: request register state\n");
    printf("\t--vcpus/-v: specify a target VCPU list\n");
}

static MiniKVMError status_parse_args(int argc, char **argv, MiniKvmStatusArgs *args) {
    MiniKVMError ret = MINI_KVM_SUCCESS;
    int32_t index = 0, name_len = 0;
    char c = 0;

    while (c != -1 && ret != MINI_KVM_ARGS_FAILED) {
        c = getopt_long(argc, argv, "n:v:rh", opts_def, &index);

        switch (c) {
        case 'n':
            name_len = strlen(optarg);
            args->name = malloc(sizeof(char) * (name_len + 1));
            strncpy(args->name, optarg, name_len + 1);
            break;
        case 'r':
            args->regs = true;
            break;
        case 'v':
            if (mini_kvm_parse_cpu_list((char *)optarg, &args->vcpus) != MINI_KVM_SUCCESS) {
                ERROR("invalid cpu list %s", optarg);
                ret = MINI_KVM_ARGS_FAILED;
            }
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

static MiniKVMError status_build_command(MiniKvmStatusArgs *args, MiniKvmStatusCommand *cmd) {
    if (args->regs) {
        cmd->type = MINI_KVM_COMMAND_SHOW_REGS;
        cmd->vcpus = args->vcpus;
    } else {
        cmd->type = MINI_KVM_COMMAND_SHOW_STATE;
    }

    return MINI_KVM_SUCCESS;
}

static MiniKVMError status_send_command(MiniKvmStatusArgs *args, MiniKvmStatusResult *res) {
    struct sockaddr_un addr = {0};
    int32_t sock = 0, ret = MINI_KVM_SUCCESS;
    MiniKvmStatusCommand cmd = {0};

    if ((sock = mini_kvm_ipc_connect(args->name, &addr)) < 0) {
        ret = MINI_KVM_FAILED_SOCKET_CREATION;
        goto out;
    }

    if (status_build_command(args, &cmd) != MINI_KVM_SUCCESS) {
        ret = MINI_KVM_STATUS_COMMAND_FAILED;
        goto close_socket;
    }

    if (mini_kvm_ipc_send_cmd(sock, &cmd, res)) {
        ret = MINI_KVM_STATUS_COMMAND_FAILED;
        goto close_socket;
    }

close_socket:
    close(sock);
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

    ret = status_send_command(&args, &res);
    if (ret != 0) {
        goto clean;
    }

    status_handle_command_result(&args, &res);

clean:
    if (args.name != NULL) {
        free(args.name);
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

    pthread_mutex_lock(&kvm->lock);
    switch (cmd->type) {
    case MINI_KVM_COMMAND_NONE: // do nothing
        break;
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
    case MINI_KVM_COMMAND_PAUSE:
        kvm->state = MINI_KVM_PAUSED;
        break;
    case MINI_KVM_COMMAND_RESUME:
        kvm->state = MINI_KVM_RUNNING;
        break;
    }
    res->cmd_type = cmd->type;
    res->vcpus = cmd->vcpus;

out:
    pthread_mutex_unlock(&kvm->lock);
    return ret;
}
