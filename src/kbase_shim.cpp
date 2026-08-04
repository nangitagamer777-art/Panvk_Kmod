#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <cstdarg>
#include "kbase_shim.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "KBaseShim", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "KBaseShim", __VA_ARGS__)

// Function pointers for original system calls
typedef int (*pfn_openat)(int dirfd, const char *pathname, int flags, ...);
typedef int (*pfn_ioctl)(int fd, int request, ...);             
static pfn_openat real_openat = nullptr;
static pfn_ioctl real_ioctl = nullptr;

static int fake_drm_fd = -1;

__attribute__((constructor))
void init_shim() {
    real_openat = (pfn_openat)dlsym(RTLD_NEXT, "openat");
    real_ioctl = (pfn_ioctl)dlsym(RTLD_NEXT, "ioctl");
    LOGI(">>> KBASE_SHIM LOADED SUCCESSFULLY <<<");
}

// Exception 1: Openat DRM Redirection
extern "C" int openat(int dirfd, const char *pathname, int flags, ...) {
    if (!real_openat) real_openat = (pfn_openat)dlsym(RTLD_NEXT, "openat");

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    // Exception condition: Target path points to standard DRM nodes
    if (pathname && (strstr(pathname, "/dev/dri/renderD128") || strstr(pathname, "/dev/dri/card"))) {
        LOGI("Intercepted DRM open: %s -> Redirecting to /dev/mali0", pathname);
        int fd = real_openat(dirfd, "/dev/mali0", flags, mode);
        if (fd >= 0) {
            fake_drm_fd = fd;
            LOGI("Device /dev/mali0 opened successfully with FD: %d", fd);
        } else {
            LOGE("Failed to open /dev/mali0: %s", strerror(errno));
        }
        return fd;
    }

    return real_openat(dirfd, pathname, flags, mode);
}

// Exception 2: Ioctl Interception and Routing
extern "C" int ioctl(int fd, int request, ...) {
    if (!real_ioctl) real_ioctl = (pfn_ioctl)dlsym(RTLD_NEXT, "ioctl");

    va_list args;
    va_start(args, request);
    void *arg = va_arg(args, void *);
    va_end(args);

    // Exception condition: Target file descriptor matches intercepted DRM handle
    if (fd == fake_drm_fd && fake_drm_fd != -1) {

        // Exception 2.1: Version spoofing
        #ifdef DRM_IOCTL_VERSION_SHIM
        if (request == DRM_IOCTL_VERSION_SHIM) {
            LOGI("Intercepted DRM_IOCTL_VERSION!");
            auto *ver = (struct drm_version_shim *)arg;
            if (ver) {
                ver->version_major = 1;
                ver->version_minor = 0;
                ver->version_patchlevel = 0;

                if (ver->name && ver->name_len > 0) {
                    const char *driver_name = "panthor";
                    size_t len = strlen(driver_name);
                    if (ver->name_len < len) len = ver->name_len;
                    strncpy(ver->name, driver_name, len);
                    ver->name_len = len;
                }
            }
            return 0;
        }
        #endif

        // Exception 2.2: Unhandled DRM ioctl logging
        LOGI("[DEBUG SHIM] Unhandled DRM IOCTL: 0x%X (type: 0x%X, nr: 0x%X)",
             request, (request >> 8) & 0xFF, request & 0xFF);

        return 0;
    }

    // Exception 3: Default pass-through to kernel
    return real_ioctl(fd, request, arg);
}
