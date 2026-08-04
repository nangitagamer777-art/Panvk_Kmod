#include "../include/kbase_shim.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int kbase_shim_init(kbase_shim_device_t *dev) {
    if (!dev) return -1;
    memset(dev, 0, sizeof(kbase_shim_device_t));

    printf("[kb] opening /dev/mali0...\n");
    dev->mali_fd = open("/dev/mali0", O_RDWR);
    if (dev->mali_fd < 0) {
        printf("[kb] open failed: %s\n", strerror(errno));
        return -1;
    }
    printf("[kb] fd %d — got it\n", dev->mali_fd);

    /* handshake */
    struct kbase_ioctl_version_check ver = { .major = 11, .minor = 11 };
    printf("[kb] version check (cmd 52)...\n");
    int res = ioctl(dev->mali_fd, KBASE_IOCTL_VERSION_CHECK, &ver);
    if (res < 0) {
        printf("[kb] version check failed: %s\n", strerror(errno));
        close(dev->mali_fd);
        dev->mali_fd = -1;
        return -1;
    }
    dev->kbase_major = ver.major;
    dev->kbase_minor = ver.minor;
    printf("[kb] handshake ok — kbase %d.%d\n", ver.major, ver.minor);

    /* create context */
    struct kbase_ioctl_set_flags flags = { .create_flags = 0 };
    printf("[kb] set_flags (cmd 1)...\n");
    res = ioctl(dev->mali_fd, KBASE_IOCTL_SET_FLAGS, &flags);
    if (res < 0) {
        printf("[kb] set_flags failed: %s\n", strerror(errno));
        close(dev->mali_fd);
        dev->mali_fd = -1;
        return -1;
    }
    printf("[kb] context created\n");

    /* grab context id */
    struct kbase_ioctl_get_context_id ctx_id = {0};
    res = ioctl(dev->mali_fd, KBASE_IOCTL_GET_CONTEXT_ID, &ctx_id);
    if (res == 0) {
        dev->context_id = ctx_id.id;
        printf("[kb] context id: %u\n", dev->context_id);
    } else {
        printf("[kb] (note) get_context_id failed: %s — not fatal\n", strerror(errno));
    }

    return 0;
}

void kbase_shim_close(kbase_shim_device_t *dev) {
    if (dev && dev->mali_fd >= 0) {
        close(dev->mali_fd);
        dev->mali_fd = -1;
        printf("[kb] session closed\n");
    }
}

int kbase_shim_get_gpu_props(kbase_shim_device_t *dev) {
    if (!dev || dev->mali_fd < 0) return -1;

    union kbase_ioctl_mem_alloc mem = {0};
    mem.in.va_pages = 2;
    mem.in.commit_pages = 2;
    mem.in.extension = 0;
    mem.in.flags = BASE_MEM_PROT_CPU_RD | BASE_MEM_PROT_CPU_WR |
                   BASE_MEM_PROT_GPU_RD | BASE_MEM_PROT_GPU_WR;

    printf("[kb] mem_alloc (cmd 5, generic)...\n");
    int res = ioctl(dev->mali_fd, KBASE_IOCTL_MEM_ALLOC, &mem);
    if (res < 0) {
        printf("[kb] mem_alloc failed: %s\n", strerror(errno));
        return -1;
    }

    printf("[kb] allocated — gpu va 0x%llx, flags out 0x%llx\n",
           (unsigned long long)mem.out.gpu_va, (unsigned long long)mem.out.flags);
    return 0;
}

int kbase_shim_create_group(kbase_shim_device_t *dev) {
    if (!dev || dev->mali_fd < 0) return -1;

    union kbase_ioctl_cs_queue_group_create grp = {0};
    grp.in.tiler_mask = ~0ULL;
    grp.in.fragment_mask = ~0ULL;
    grp.in.compute_mask = ~0ULL;
    grp.in.cs_min = 1;
    grp.in.priority = 0;
    grp.in.tiler_max = 8;
    grp.in.fragment_max = 8;
    grp.in.compute_max = 8;
    grp.in.csi_handlers = 0;

    printf("[kb] queue group create (cmd 58)...\n");
    int res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_GROUP_CREATE, &grp);
    if (res < 0) {
        printf("[kb] group create failed: %s\n", strerror(errno));
        return -1;
    }

    printf("[kb] group created — handle %u, uid %u\n",
           grp.out.group_handle, grp.out.group_uid);
    dev->group_handle = grp.out.group_handle;
    return 0;
}

/* ---- v1 and v2 kept for reference, not called by main ---- */

int kbase_shim_register_and_bind_queue(kbase_shim_device_t *dev) {
    if (!dev || dev->mali_fd < 0) return -1;

    union kbase_ioctl_mem_alloc mem = {0};
    mem.in.va_pages = 2;
    mem.in.commit_pages = 2;
    mem.in.extension = 0;
    mem.in.flags = BASE_MEM_PROT_CPU_RD | BASE_MEM_PROT_CPU_WR |
                   BASE_MEM_PROT_GPU_RD | BASE_MEM_PROT_GPU_WR;

    printf("[kb v1] allocating queue buffer...\n");
    int res = ioctl(dev->mali_fd, KBASE_IOCTL_MEM_ALLOC, &mem);
    if (res < 0) {
        printf("[kb v1] mem_alloc failed: %s\n", strerror(errno));
        return -1;
    }
    dev->queue_buffer_va = mem.out.gpu_va;
    printf("[kb v1] queue buffer at gpu va 0x%llx\n",
           (unsigned long long)dev->queue_buffer_va);

    struct kbase_ioctl_cs_queue_register reg = {0};
    reg.buffer_gpu_addr = dev->queue_buffer_va;
    reg.buffer_size = 8192;
    reg.priority = 0;

    printf("[kb v1] cs_queue_register (cmd 36)...\n");
    res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_REGISTER, &reg);
    if (res < 0) {
        printf("[kb v1] register failed: %s\n", strerror(errno));
        return -1;
    }
    printf("[kb v1] queue registered\n");

    union kbase_ioctl_cs_queue_bind bind = {0};
    bind.in.buffer_gpu_addr = dev->queue_buffer_va;
    bind.in.group_handle = dev->group_handle;
    bind.in.csi_index = 0;

    printf("[kb v1] cs_queue_bind (cmd 39, group=%u)...\n", dev->group_handle);
    res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_BIND, &bind);
    if (res < 0) {
        printf("[kb v1] bind failed: %s\n", strerror(errno));
        return -1;
    }
    dev->queue_mmap_handle = bind.out.mmap_handle;
    printf("[kb v1] bound — mmap_handle 0x%llx\n",
           (unsigned long long)dev->queue_mmap_handle);

    return 0;
}

#include <sys/mman.h>

int kbase_shim_register_and_bind_queue_v2(kbase_shim_device_t *dev) {
    if (!dev || dev->mali_fd < 0) return -1;

    union kbase_ioctl_mem_alloc mem = {0};
    mem.in.va_pages = 2;
    mem.in.commit_pages = 2;
    mem.in.extension = 0;
    mem.in.flags = BASE_MEM_PROT_CPU_RD | BASE_MEM_PROT_CPU_WR |
                   BASE_MEM_PROT_GPU_RD | BASE_MEM_PROT_GPU_WR;

    printf("\n[kb v2] allocating queue buffer...\n");
    int res = ioctl(dev->mali_fd, KBASE_IOCTL_MEM_ALLOC, &mem);
    if (res < 0) return -1;

    dev->queue_buffer_va = mem.out.gpu_va;
    printf("[kb v2] queue buffer at gpu va 0x%llx\n", (unsigned long long)dev->queue_buffer_va);

    void *cpu_queue_ptr = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED,
                               dev->mali_fd, dev->queue_buffer_va);
    if (cpu_queue_ptr == MAP_FAILED) {
        printf("[kb v2] mmap warning: %s\n", strerror(errno));
    } else {
        printf("[kb v2] queue mapped at %p (pages pinned)\n", cpu_queue_ptr);
        memset(cpu_queue_ptr, 0, 8192);
    }

    struct kbase_ioctl_cs_queue_register reg = {0};
    reg.buffer_gpu_addr = dev->queue_buffer_va;
    reg.buffer_size = 8192;
    reg.priority = 0;

    printf("[kb v2] cs_queue_register (size=8192)...\n");
    res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_REGISTER, &reg);

    if (res < 0) {
        printf("[kb v2] failed with 8192 bytes: %d, retrying with size=2 (pages)...\n", errno);
        reg.buffer_size = 2;
        res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_REGISTER, &reg);
        if (res < 0) {
            printf("[kb v2] register critically failed: %s\n", strerror(errno));
            return -1;
        }
    }
    printf("[kb v2] queue registered in kernel\n");

    union kbase_ioctl_cs_queue_bind bind = {0};
    bind.in.buffer_gpu_addr = dev->queue_buffer_va;
    bind.in.group_handle = dev->group_handle;
    bind.in.csi_index = 0;

    printf("[kb v2] cs_queue_bind...\n");
    res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_BIND, &bind);
    if (res < 0) {
        printf("[kb v2] bind failed: %s\n", strerror(errno));
        return -1;
    }

    dev->queue_mmap_handle = bind.out.mmap_handle;
    printf("[kb v2] bound — mmap_handle 0x%llx\n", (unsigned long long)dev->queue_mmap_handle);

    return 0;
}
