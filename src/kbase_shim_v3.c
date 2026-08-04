#include "kbase_shim.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

int kbase_shim_register_and_bind_queue_v3(kbase_shim_device_t *dev) {
    if (!dev || dev->mali_fd < 0) return -1;

    /* mem_alloc with GPU_EX */
    union kbase_ioctl_mem_alloc mem = {0};
    mem.in.va_pages = 2;
    mem.in.commit_pages = 2;
    mem.in.extension = 0;
    mem.in.flags = BASE_MEM_PROT_CPU_RD | BASE_MEM_PROT_CPU_WR |
                   BASE_MEM_PROT_GPU_RD | BASE_MEM_PROT_GPU_EX;

    printf("\n[kb v3] mem_alloc with GPU_EX...\n");
    printf("[kb v3] flags in = 0x%llx (CPU_RD|CPU_WR|GPU_RD|GPU_EX)\n",
           (unsigned long long)mem.in.flags);

    int res = ioctl(dev->mali_fd, KBASE_IOCTL_MEM_ALLOC, &mem);
    if (res < 0) {
        printf("[kb v3] mem_alloc failed: %s\n", strerror(errno));
        return -1;
    }

    dev->queue_buffer_va = mem.out.gpu_va;
    printf("[kb v3] queue buffer at gpu va 0x%llx, flags out 0x%llx\n",
           (unsigned long long)dev->queue_buffer_va, (unsigned long long)mem.out.flags);

    /* register */
    struct kbase_ioctl_cs_queue_register reg = {0};
    reg.buffer_gpu_addr = dev->queue_buffer_va;
    reg.buffer_size = 0x2000;
    reg.priority = 0;

    printf("[kb v3] cs_queue_register: addr=0x%llx size=0x%x prio=%u\n",
           (unsigned long long)reg.buffer_gpu_addr, reg.buffer_size, reg.priority);

    res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_REGISTER, &reg);
    if (res < 0) {
        printf("[kb v3] register failed: %s\n", strerror(errno));
        return -1;
    }

    printf("[kb v3] registered\n");

    /* bind */
    union kbase_ioctl_cs_queue_bind bind = {0};
    bind.in.buffer_gpu_addr = dev->queue_buffer_va;
    bind.in.group_handle = dev->group_handle;
    bind.in.csi_index = 0;

    printf("[kb v3] cs_queue_bind: addr=0x%llx group=%u csi=%u\n",
           (unsigned long long)bind.in.buffer_gpu_addr,
           bind.in.group_handle, bind.in.csi_index);

    res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_BIND, &bind);
    if (res < 0) {
        printf("[kb v3] bind failed: %s\n", strerror(errno));
        return -1;
    }

    dev->queue_mmap_handle = bind.out.mmap_handle;
    printf("[kb v3] bound — mmap_handle 0x%llx\n",
           (unsigned long long)dev->queue_mmap_handle);

    return 0;
}
