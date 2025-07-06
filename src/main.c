#include <string.h>

#include "utils/logger.h"

int main() {
    logger_init(NULL);
    INFO("minikvm initialization");

    return 0;
}
