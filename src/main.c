#include <stdio.h>
#include <string.h>

#include "commands/commands.h"
#include "core/errors.h"
#include "core/logger.h"

const MiniKVMCommand commands[] = {{"pause", mini_kvm_pause},       {"resume", mini_kvm_resume},
                                   {"run", mini_kvm_run},           {"status", mini_kvm_status},
                                   {"shutdown", mini_kvm_shutdown}, {NULL, NULL}};

void print_help() {
    printf("USAGE:\n");
    printf("\tmini_kvm <run|pause|resume|shutdown|status>\n");
}

MiniKVMError handle_command(int32_t argc, char **argv) {
    int32_t cmd_idx = 0;
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

int32_t main(int32_t argc, char **argv) {
    logger_init(NULL);

    if (argc <= 1 || strncmp("-h", argv[1], 2) == 0 || strncmp("--help", argv[1], 6) == 0) {
        print_help();
        return MINI_KVM_SUCCESS;
    }

    return handle_command(argc - 1, argv + 1);
}
