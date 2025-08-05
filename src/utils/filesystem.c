#include "filesystem.h"

#include "utils/logger.h"

#define _XOPEN_SOURCE 500
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

int nftw_callback(const char *fpath, const struct stat *st, int typeflags, struct FTW *ftwbuf) {
    int ret = remove(fpath);
    if (ret != 0) {
        ERROR("failed to remove %s (%s)", fpath, strerror(errno));
    }
    return ret;
}

int32_t rmrf(const char *path) { return nftw(path, nftw_callback, 10, FTW_DEPTH | FTW_PHYS); }
