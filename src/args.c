#include "args.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

MiniKVMArgs *parse_args(int argc, char **argv) {
    int32_t index = 1;
    MiniKVMArgs *args = malloc(sizeof(MiniKVMArgs));
    args->img_path = NULL;
    args->log_file_path = NULL;

    while (index < argc) {

        // TODO: handle case where `-I` is the last argument (argv[index++] == NULL)
        if (strncmp(argv[index], "-I", 2) == 0 && args->img_path == NULL) {
            index++;
            if (access(argv[index], F_OK)) { // file not found
                printf("-I: bad argument (%s does not exist)\n", argv[index]);
                goto parse_args_parse_error;
            }

            // file found copy path to args
            uint64_t img_path_len = strlen(argv[index]);
            args->img_path = malloc(sizeof(char) * (1 + img_path_len));
            strncpy(args->img_path, argv[index], img_path_len + 1);
        } else if (strncmp(argv[index], "-D", 2) == 0) {
            index++;
            if (index == argc || argv[index][0] == '-') {
                printf("-D: bad argument\n");
                goto parse_args_parse_error;
            }

            uint64_t log_path_len = strlen(argv[index]);
            args->log_file_path = malloc(sizeof(char) * (1 + log_path_len));
            strncpy(args->log_file_path, argv[index], log_path_len + 1);
        } else {
            printf("unknown arguments %s\n", argv[index]);
            goto parse_args_usage;
        }

        index++;
    }

    if (args->img_path == NULL) {
        printf("error: no image path were given\n");
        goto parse_args_usage;
    }

    return args;

parse_args_usage:
    printf("\nUsage: mini_kvm -I FILEPATH\n");
parse_args_parse_error:
    free_parse_args(args);

    return NULL;
}

void free_parse_args(MiniKVMArgs *args) {
    free(args->img_path);
    free(args->log_file_path);
    free(args);
}
