#include "status.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands.h"
#include "constants.h"
#include "utils/errors.h"
#include "utils/logger.h"

static const struct option opts_def[] = {
    {"name", required_argument, NULL, 'n'}, {"help", no_argument, NULL, 'h'}, {0, 0, 0, 0}};

void status_print_help() {
    printf("USAGE: mini_kvm run\n");
    printf("\t--name/-n: set the name of the virtual machine\n");
}

static int status_parse_args(int argc, char **argv, MiniKvmStatusArgs *args) {
    int32_t ret = MINI_KVM_SUCCESS, index;
    char c = 0;
    uint32_t name_len = 0;

    while (c != -1 && ret != MINI_KVM_ARGS_FAILED) {
        c = getopt_long(argc, argv, "n:h", opts_def, &index);

        switch (c) {
        case 'n':
            name_len = strlen(optarg);
            args->name = malloc(sizeof(char) * (name_len + 1));
            strncpy(args->name, optarg, name_len + 1);
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

    return ret;
}

static int32_t status_open_dir(const char *path) {
    int32_t root_fs_dir = open(MINI_KVM_FS_ROOT_PATH, O_DIRECTORY | O_RDONLY);
    if (root_fs_dir < 0) {
        INFO("unable to open %s (%s)", MINI_KVM_FS_ROOT_PATH, strerror(errno));
        return -1;
    }
    int32_t vm_fs_dir = openat(root_fs_dir, path, O_DIRECTORY | O_RDONLY);
    if (vm_fs_dir < 0) {
        INFO("%s is not running, exiting", MINI_KVM_FS_ROOT_PATH);
        return -1;
    }

    return vm_fs_dir;
}

int mini_kvm_status(int argc, char **argv) {
    int ret = MINI_KVM_SUCCESS;
    MiniKvmStatusArgs args = {0};
    ret = status_parse_args(argc, argv, &args);
    if (ret != 0) {
        goto clean;
    }

    if (args.name == NULL) {
        INFO("status: no name was specified, exiting ...");
        goto clean;
    }

    int32_t vm_dir = status_open_dir(args.name);
    if (vm_dir < 0) {
        goto clean;
    }

    char *pidfile_name = malloc(sizeof(char) * (strlen(args.name) + 5));
    sprintf(pidfile_name, "%s.pid", args.name);
    int32_t pidfile = openat(vm_dir, pidfile_name, O_RDONLY);
    if (pidfile < 0) {
        ERROR("status: unable to find %s pidfile", args.name);
        goto clean;
    }

    int32_t vm_pid = 0;
    read(pidfile, &vm_pid, sizeof(int32_t));
    TRACE("%s is running with pid %d", args.name, vm_pid);

clean:
    if (args.name != NULL) {
        close(pidfile);
        close(vm_dir);
        free(args.name);
    }

    return ret;
}
