#include "args.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void copy_argv_str(char **dst, const char *src) {
    uint64_t src_len = strlen(src);
    *dst = malloc(sizeof(char) * (1 + src_len));
    strncpy(*dst, src, src_len + 1);
}

static int32_t parse_str(int32_t argc, char **argv, int32_t index, char **args_dst) {
    if (index == argc || argv[index][0] == '-') {
        printf("%s: bad argument\n", argv[index - 1]);
        return -1;
    }

    copy_argv_str(args_dst, argv[index]);
    return 0;
}

static int32_t parse_filepath(int32_t argc, char **argv, int32_t index, char **args_dst) {
    if (index == argc || access(argv[index], F_OK)) {
        printf("%s: bad argument (%s does not exist)\n", argv[index - 1], argv[index]);
        return -1;
    }

    copy_argv_str(args_dst, argv[index]);
    return 0;
}

MiniKVMArgs *parse_args(int32_t argc, char **argv) {
    int32_t index = 1;
    MiniKVMArgs *args = malloc(sizeof(MiniKVMArgs));
    args->img_path = NULL;
    args->log_file_path = NULL;

    while (index < argc) {

        // TODO: handle case where `-I` is the last argument (argv[index++] == NULL)
        if (strncmp(argv[index], "-I", 2) == 0 && args->img_path == NULL) {
            if (parse_filepath(argc, argv, ++index, &args->img_path)) {
                goto parse_args_parse_error;
            }
        } else if (strncmp(argv[index], "-D", 2) == 0 && args->log_file_path == NULL) {
            if (parse_str(argc, argv, ++index, &args->log_file_path)) {
                goto parse_args_parse_error;
            }
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
