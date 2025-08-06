#include "resume.h"

#include "commands.h"
#include "commands/status.h"
#include "ipc/ipc.h"
#include "utils/errors.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

static const struct option opts_def[] = {
    {"name", required_argument, NULL, 'n'}, {"help", no_argument, NULL, 'h'}, {0, 0, 0, 0}};

static void resume_print_help() {
    printf("USAGE: mini_kvm resume\n");
    printf("\t--name/-n: set the name of the virtual machine\n");
}

static MiniKVMError resume_parse_args(int argc, char **argv, MiniKvmResumeArgs *args) {
    MiniKVMError ret = MINI_KVM_SUCCESS;
    int32_t index = 0, name_len = 0;
    char c = 0;

    while (c != -1 && ret != MINI_KVM_ARGS_FAILED) {
        c = getopt_long(argc, argv, "n:h", opts_def, &index);

        switch (c) {
        case 'n':
            name_len = strlen(optarg);
            args->name = malloc(sizeof(char) * (name_len + 1));
            strncpy(args->name, optarg, name_len + 1);
            break;
        case 'h':
            resume_print_help();
            ret = 1;
            break;
        case '?':
            ret = MINI_KVM_ARGS_FAILED;
            break;
        }
    }

    return ret;
}

MiniKVMError mini_kvm_resume(int argc, char **argv) {
    MiniKVMError ret = MINI_KVM_SUCCESS;
    MiniKvmResumeArgs args = {0};
    int32_t sock = 0;
    struct sockaddr_un addr = {0};
    MiniKvmStatusCommand cmd = {0};
    MiniKvmStatusResult res = {0};

    ret = resume_parse_args(argc, argv, &args);
    if (ret != MINI_KVM_SUCCESS) {
        resume_print_help();
        goto out;
    }

    if (args.name == NULL) {
        INFO("resume: no name was specified, exiting ...");
        goto clean;
    }

    if (mini_kvm_check_vm(args.name) < 0) {
        INFO("resume: VM %s is not running, exiting ...", args.name);
        goto clean;
    }

    if ((sock = mini_kvm_ipc_connect(args.name, &addr)) < 0) {
        goto clean;
    }

    cmd.type = MINI_KVM_COMMAND_SHOW_STATE;
    if (mini_kvm_ipc_send_cmd(sock, &cmd, &res) < 0) {
        goto close_socket;
    }

    if (res.state == MINI_KVM_RUNNING) {
        goto close_socket;
    }

    cmd.type = MINI_KVM_COMMAND_RESUME;
    if (mini_kvm_ipc_send_cmd(sock, &cmd, &res) < 0) {
        goto close_socket;
    }

    INFO("VM %s successfuly resumed", args.name);

close_socket:
    close(sock);

clean:
    free(args.name);
out:
    return ret;
}
