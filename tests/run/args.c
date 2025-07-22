#include <stdio.h>

#include "commands/run.h"

extern int run_parser_args(int argc, char **argv, MiniKvmRunArgs *args);

void print_args(MiniKvmRunArgs *args) {
    printf("log_enabled=%d\n", args->log_enabled);
    printf("vcpu=%u\n", args->vcpu);
    printf("mem_size=%lu\n", args->mem_size);
}

int main(int argc, char **argv) {
    MiniKvmRunArgs args = {0};

    run_parser_args(argc, argv, &args);
    print_args(&args);

    return 0;
}
