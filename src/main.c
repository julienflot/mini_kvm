#include <stdio.h>
#include <string.h>

#include "commands.h"
#include "utils/errors.h"
#include "utils/logger.h"

const MiniKVMCommand commands[] = {
    {"run", mini_kvm_run}, {"status", mini_kvm_status}, {NULL, NULL}};

void print_help() { printf("mini_kvm <run|pause|resume|status>\n"); }

int handle_command(int argc, char **argv) {
    int cmd_idx = 0;
    char *name = NULL;
    while ((name = commands[cmd_idx].name) != NULL) {
        if (strncmp(name, argv[0], strlen(name)) == 0) {
            return commands[cmd_idx].run(argc, argv);
        }

        cmd_idx++;
    }

    fprintf(stderr, "Unknown subcommand: %s\n\n", argv[0]);
    print_help();

    return MINI_KVM_UNRECOGNIZED_COMMAND;
}

int main(int argc, char **argv) {
    logger_init(NULL);

    // TODO: do a better check
    if (argc <= 1) {
        print_help();
        return MINI_KVM_SUCCESS;
    }

    return handle_command(argc - 1, argv + 1);
}
