#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>

int mprotect(void *addr, size_t len, int prot) {
    typeof(mprotect) *real = dlsym(RTLD_NEXT, "mprotect");
    int ret = real(addr, len, prot);
    if (ret < 0 && errno == EPERM) {
        fprintf(stderr, "[scudo_fix] mprotect EPERM ignored for %p\n", addr);
        return 0; /* Pretend success */
    }
    return ret;
}
