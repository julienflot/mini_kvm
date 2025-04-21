#ifndef MINI_KVM_ARGS_H
#define MINI_KVM_ARGS_H

typedef struct MiniKVMArgs {
    char *img_path;
    char *log_file_path;
} MiniKVMArgs;

MiniKVMArgs *parse_args(int argc, char **argv);
void free_parse_args(MiniKVMArgs *args);

#endif /* MINI_KVM_ARGS_H */
