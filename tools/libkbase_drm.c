#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

/* ── Real libdrm functions ── */
static void *get_real(const char *name) {
    static void *handle;
    if (!handle) handle = dlopen("libdrm.so", RTLD_NOW);
    return dlsym(handle ? handle : RTLD_DEFAULT, name);
}

/* ── Intercepted functions ── */

/* drmGetDevices2 - inject /dev/mali0 as fake DRM device */
int drmGetDevices2(uint32_t flags, void **devices, int max) {
    typeof(drmGetDevices2) *real = get_real("drmGetDevices2");
    return real ? real(flags, devices, max) : 0;
}

/* drmSyncobjCreate - succeed silently */
int drmSyncobjCreate(int fd, uint32_t flags, uint32_t *handle) {
    fprintf(stderr, "[kbase_drm] drmSyncobjCreate stub\n");
    static uint32_t next = 1000;
    *handle = next++;
    return 0;
}

/* drmSyncobjDestroy - succeed silently */
int drmSyncobjDestroy(int fd, uint32_t handle) {
    fprintf(stderr, "[kbase_drm] drmSyncobjDestroy stub (handle=%u)\n", handle);
    return 0;
}

/* drmSyncobjWait - pretend it's already signaled */
int drmSyncobjWait(int fd, uint32_t *handles, unsigned num, int64_t timeout, unsigned flags, uint32_t *first) {
    fprintf(stderr, "[kbase_drm] drmSyncobjWait stub (returns success)\n");
    return 0;
}

/* drmSyncobjTimelineWait - pretend signaled */
int drmSyncobjTimelineWait(int fd, uint32_t *handles, uint64_t *points, unsigned num, int64_t timeout, unsigned flags, uint32_t *first) {
    fprintf(stderr, "[kbase_drm] drmSyncobjTimelineWait stub\n");
    return 0;
}

/* drmSyncobjExportSyncFile - return a dummy fd */
int drmSyncobjExportSyncFile(int fd, uint32_t handle, int *sync_file_fd) {
    fprintf(stderr, "[kbase_drm] drmSyncobjExportSyncFile stub\n");
    *sync_file_fd = open("/dev/null", O_RDONLY);
    return *sync_file_fd >= 0 ? 0 : -1;
}

/* drmSyncobjImportSyncFile - succeed */
int drmSyncobjImportSyncFile(int fd, uint32_t handle, int sync_file_fd) {
    fprintf(stderr, "[kbase_drm] drmSyncobjImportSyncFile stub\n");
    close(sync_file_fd);
    return 0;
}

/* drmIoctl - let real ones through, fake success for unknown ones */
int drmIoctl(int fd, unsigned long request, void *arg) {
    typeof(drmIoctl) *real = get_real("drmIoctl");
    if (real) { int r = real(fd, request, arg); if (r == 0 || errno != EINVAL) return r; }
    fprintf(stderr, "[kbase_drm] drmIoctl stub (request=0x%lx)\n", request);
    return 0;
}

/* drmCloseBufferHandle - succeed */
int drmCloseBufferHandle(int fd, uint32_t handle) {
    fprintf(stderr, "[kbase_drm] drmCloseBufferHandle stub (handle=%u)\n", handle);
    return 0;
}

/* drmPrimeFDToHandle - succeed with fake handle */
int drmPrimeFDToHandle(int fd, int prime_fd, uint32_t *handle) {
    fprintf(stderr, "[kbase_drm] drmPrimeFDToHandle stub\n");
    static uint32_t next = 5000;
    *handle = next++;
    close(prime_fd);
    return 0;
}

/* drmPrimeHandleToFD - return dummy fd */
int drmPrimeHandleToFD(int fd, uint32_t handle, uint32_t flags, int *prime_fd) {
    fprintf(stderr, "[kbase_drm] drmPrimeHandleToFD stub\n");
    *prime_fd = open("/dev/null", O_RDONLY);
    return *prime_fd >= 0 ? 0 : -1;
}
