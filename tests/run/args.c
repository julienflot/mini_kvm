#include <stdio.h>

#include "commands/run.h"

extern int run_parse_args(int argc, char **argv, MiniKvmRunArgs *args);

void print_args(MiniKvmRunArgs *args) {
    printf("log_enabled=%d\n", args->log_enabled);
    printf("vcpu=%u\n", args->vcpu);
    printf("mem_size=%lu\n", args->mem_size);
    printf("name=%s\n", args->name);
}

int main(int argc, char **argv) {
    MiniKvmRunArgs args = {0};

    run_parse_args(argc, argv, &args);
    print_args(&args);

    return 0;
}
