#include "status.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "commands.h"
#include "constants.h"
#include "kvm/kvm.h"
#include "utils/errors.h"
#include "utils/logger.h"
#include "utils/utils.h"

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

static int status_parse_args(int argc, char **argv, MiniKvmStatusArgs *args) {
    int32_t ret = MINI_KVM_SUCCESS, index = 0, name_len = 0;
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

static int32_t status_open_dir(const char *path) {
    int32_t root_fs_dir = open(MINI_KVM_FS_ROOT_PATH, O_DIRECTORY | O_RDONLY);
    if (root_fs_dir < 0) {
        return -MINI_KVM_INTERNAL_ERROR;
    }
    int32_t vm_fs_dir = openat(root_fs_dir, path, O_DIRECTORY | O_RDONLY);
    if (vm_fs_dir < 0) {
        return -MINI_KVM_INTERNAL_ERROR;
    }

    return vm_fs_dir;
}

static int32_t status_send_command(MiniKvmStatusArgs *args, MiniKvmStatusResult *res) {
    struct sockaddr_un addr;
    int32_t sock = 0, ret = MINI_KVM_SUCCESS;
    socklen_t socket_size = sizeof(struct sockaddr_un);
    MiniKvmStatusCommand cmd = {0};
    char *socket_name = NULL;

    addr.sun_family = AF_UNIX;
    socket_name =
        malloc(sizeof(char) * (strlen(args->name) * 2 + strlen(MINI_KVM_FS_ROOT_PATH) + 8));
    sprintf(socket_name, "%s/%s/%s.sock", MINI_KVM_FS_ROOT_PATH, args->name, args->name);
    strncpy(addr.sun_path, socket_name, sizeof(addr.sun_path));

    if ((sock = socket(addr.sun_family, SOCK_STREAM, 0)) < 0) {
        ERROR("unable to create status socket (%s)", strerror(errno));
        ret = MINI_KVM_INTERNAL_ERROR;
        goto cleanup;
    }

    if (connect(sock, (struct sockaddr *)&addr, socket_size)) {
        ERROR("unable to connect status socket %s (%s)", args->name, strerror(errno));
        ret = MINI_KVM_INTERNAL_ERROR;
        goto close_socket;
    }

    if (args->regs) {
        cmd.type = MINI_KVM_COMMAND_REGS;
        cmd.vcpus = args->vcpus;
    } else {
        cmd.type = MINI_KVM_COMMAND_RUNNING;
    }

    if (send(sock, &cmd, sizeof(cmd), 0) < 0) {
        ERROR("unable to send status socket %s (%s)", args->name, strerror(errno));
        ret = MINI_KVM_INTERNAL_ERROR;
        goto close_socket;
    }

    if (recv(sock, res, sizeof(MiniKvmStatusResult), 0) == -1) {
        ERROR("unable to recv msg on %s (%s)", args->name, strerror(errno));
        ret = MINI_KVM_INTERNAL_ERROR;
        goto close_socket;
    }

close_socket:
    close(sock);
cleanup:
    free(socket_name);
    return ret;
}

int32_t mini_kvm_status(int argc, char **argv) {
    int32_t ret = MINI_KVM_SUCCESS, vm_pid = -1, bytes_read = 0;
    int32_t vm_dir = -1, pidfile = -1;
    char *pidfile_name = NULL;
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

    vm_dir = status_open_dir(args.name);
    if (vm_dir < 0) {
        INFO("status: VM %s is not running", args.name);
        goto clean;
    }

    pidfile_name = malloc(sizeof(char) * (strlen(args.name) + 5));
    sprintf(pidfile_name, "%s.pid", args.name);
    pidfile = openat(vm_dir, pidfile_name, O_RDONLY);
    if (pidfile < 0) {
        ERROR("status: unable to find %s pidfile", args.name);
        goto clean;
    }

    vm_pid = -1;
    bytes_read = read(pidfile, &vm_pid, sizeof(int32_t));
    if (bytes_read <= 0 || kill(vm_pid, 0) != 0) {
        INFO("status: VM %s is not running", args.name);
        goto clean;
    }

    ret = status_send_command(&args, &res);
    if (ret != 0) {
        goto clean;
    }
    TRACE("status: %s status: %d", args.name, res.state);

clean:
    if (args.name != NULL) {
        free(pidfile_name);
        close(pidfile);
        close(vm_dir);
        free(args.name);
    }

    return ret;
}

static void *status_thread_func(void *args) {
    struct sockaddr_un socket_addr = {0}, remote_addr = {0};
    socklen_t socket_size = sizeof(struct sockaddr_un);
    int32_t status_socket = 0, remote_sock = 0;
    char *socket_name = NULL;
    MiniKvmStatusCommand cmd = {0};
    MiniKvmStatusResult res = {0};
    Kvm *kvm = (Kvm *)args;

    socket_addr.sun_family = AF_UNIX;
    socket_name = malloc(sizeof(char) * (strlen(kvm->fs_path) + 7 + strlen(kvm->name)));
    sprintf(socket_name, "%s/%s.sock", kvm->fs_path, kvm->name);
    strncpy(socket_addr.sun_path, socket_name, sizeof(socket_addr.sun_path));

    status_socket = socket(socket_addr.sun_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (status_socket < 0) {
        ERROR("unable to create status socket (%s)", strerror(errno));
        goto cleanup;
    }

    if (bind(status_socket, (struct sockaddr *)&socket_addr, socket_size) < 0) {
        ERROR("unable to bind to socket (%s)", strerror(errno));
        goto close_socket;
    }

    if (listen(status_socket, 0) < 0) {
        ERROR("unable to listen to socket (%s)", strerror(errno));
        goto close_socket;
    }

    while (!kvm->shutdown_status_thread) {
        memset(&cmd, 0, sizeof(cmd));

        remote_sock = accept(status_socket, (struct sockaddr *)&remote_addr, &socket_size);
        if (remote_sock < 0 && errno == EAGAIN) {
            usleep(100000);
            continue;
        }

        if (remote_sock < 0) {
            ERROR("unable to accept connection (%s)", strerror(errno));
            continue;
        }
        recv(remote_sock, &cmd, sizeof(cmd), 0);

        memset(&res, 0, sizeof(res));
        mini_kvm_status_handle_command(kvm, &cmd, &res);

        send(remote_sock, &res, sizeof(res), 0);
    }

close_socket:
    close(status_socket);
cleanup:
    free(socket_name);

    return NULL;
}

int32_t mini_kvm_start_status_thread(Kvm *kvm) {
    int32_t ret = MINI_KVM_SUCCESS;

    kvm->shutdown_status_thread = false;
    ret = pthread_create(&kvm->status_thread, NULL, status_thread_func, (void *)kvm);
    if (ret != 0) {
        ERROR("unable to start status thread (%s)", strerror(errno));
        ret = MINI_KVM_INTERNAL_ERROR;
    }

    return ret;
}

int32_t mini_kvm_status_handle_command(Kvm *kvm, MiniKvmStatusCommand *cmd,
                                       MiniKvmStatusResult *res) {
    pthread_mutex_lock(&kvm->lock);
    switch (cmd->type) {
    case MINI_KVM_COMMAND_NONE: // do nothing
        break;
    case MINI_KVM_COMMAND_RUNNING:
        res->state = kvm->state;
        break;
    case MINI_KVM_COMMAND_REGS:
        for (uint32_t index = 0; index < kvm->vcpu_count; index++) {
            if (!(cmd->vcpus & (1 << index))) {
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
        break;
    }
    pthread_mutex_unlock(&kvm->lock);

    return MINI_KVM_SUCCESS;
}
