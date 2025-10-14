#include "ipc.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "utils/logger.h"

static socklen_t SOCKET_SIZE = sizeof(struct sockaddr_un);

int32_t mini_kvm_ipc_create_socket(Kvm *kvm, struct sockaddr_un *addr) {
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

int32_t mini_kvm_ipc_receive_cmd(Kvm *kvm) {
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

int32_t mini_kvm_ipc_connect(char *name, struct sockaddr_un *addr) {
    int32_t sock = 0;

    addr->sun_family = AF_UNIX;
    sprintf(addr->sun_path, "%s/%s/%s.sock", MINI_KVM_FS_ROOT_PATH, name, name);

    if ((sock = socket(addr->sun_family, SOCK_STREAM, 0)) < 0) {
        ERROR("unable to create status socket (%s)", strerror(errno));
        goto out;
    }

    if (connect(sock, (struct sockaddr *)addr, SOCKET_SIZE)) {
        ERROR("unable to connect status socket %s (%s)", name, strerror(errno));
        goto fail_socket;
    }

    goto out;

fail_socket:
    close(sock);

out:
    return sock;
}

int32_t mini_kvm_ipc_send_cmd(int32_t sock, MiniKvmStatusCommand *cmd, MiniKvmStatusResult *res) {
    if (send(sock, cmd, sizeof(MiniKvmStatusCommand), 0) < 0) {
        ERROR("unable to send command to status socket (%s)", strerror(errno));
        return -1;
    }

    if (recv(sock, res, sizeof(MiniKvmStatusResult), 0) < -1) {
        ERROR("unable to recv msg on status socket (%s)", strerror(errno));
        return -1;
    }

    return 0;
}
