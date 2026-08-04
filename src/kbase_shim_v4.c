#include "kbase_shim.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* user i/o page layout */
#define BASEP_QUEUE_NR_MMAP_USER_PAGES 3
#define PAGE_SIZE_4K 4096

#define CS_INSERT_LO  0x0000
#define CS_INSERT_HI  0x0004
#define CS_EXTRACT_LO 0x0000
#define CS_EXTRACT_HI 0x0004
#define CS_ACTIVE      0x0008

/* write a tiny CSF program into the queue buffer */
static int write_csf_instructions(void *cpu_ptr, size_t buffer_size) {
    if (!cpu_ptr || buffer_size < 32) return -1;

    __u64 *instr = (__u64 *)cpu_ptr;
    int i = 0;

    printf("[csf] writing instructions:\n");

    instr[i++] = CSF_INSTR(CSF_OPCODE_NOP, 0);
    printf("[csf]   [%d] NOP           = 0x%016llx\n", i-1, (unsigned long long)instr[i-1]);

    instr[i++] = CSF_INSTR(CSF_OPCODE_MOVE32, 0xDEAD0000);
    printf("[csf]   [%d] MOVE32 0xDEAD = 0x%016llx\n", i-1, (unsigned long long)instr[i-1]);

    instr[i++] = CSF_INSTR(CSF_OPCODE_WAIT, 0x100);
    printf("[csf]   [%d] WAIT 0x100    = 0x%016llx\n", i-1, (unsigned long long)instr[i-1]);

    instr[i++] = CSF_INSTR(CSF_OPCODE_NOP, 0);
    printf("[csf]   [%d] NOP           = 0x%016llx\n", i-1, (unsigned long long)instr[i-1]);

    instr[i++] = CSF_INSTR(CSF_OPCODE_END, 0);
    printf("[csf]   [%d] END           = 0x%016llx\n", i-1, (unsigned long long)instr[i-1]);

    printf("[csf] %d instructions (%d bytes)\n", i, i * 8);
    return 0;
}

/* poll CS_ACTIVE until the queue goes idle or we time out */
static int wait_for_completion(void *user_io_ptr, int timeout_ms) {
    volatile __u32 *output_page = (volatile __u32 *)((char *)user_io_ptr + 2 * PAGE_SIZE_4K);
    int waited = 0;

    printf("[csf] polling CS_ACTIVE every 10ms (timeout %dms)...\n", timeout_ms);

    while (waited < timeout_ms) {
        __u32 active = output_page[CS_ACTIVE / 4];
        __u32 extract_lo = output_page[CS_EXTRACT_LO / 4];
        __u32 extract_hi = output_page[CS_EXTRACT_HI / 4];

        if (waited % 100 == 0) {
            printf("[csf]   t=%dms CS_ACTIVE=0x%08x EXTRACT=0x%08x_%08x\n",
                   waited, active, extract_hi, extract_lo);
        }

        if (!(active & 0x1)) {
            printf("[csf] queue done — t=%dms CS_ACTIVE=0x%08x EXTRACT=0x%08x_%08x\n",
                   waited, active, extract_hi, extract_lo);
            return 0;
        }

        usleep(10000);
        waited += 10;
    }

    printf("[csf] timeout — CS_ACTIVE still hot after %dms\n", timeout_ms);
    return -1;
}

int kbase_shim_csf_kick_and_wait_v4(kbase_shim_device_t *dev) {
    if (!dev || dev->mali_fd < 0) return -1;

    /* ---- mem_alloc with GPU_EX ---- */
    union kbase_ioctl_mem_alloc mem = {0};
    mem.in.va_pages = 2;
    mem.in.commit_pages = 2;
    mem.in.extension = 0;
    mem.in.flags = BASE_MEM_PROT_CPU_RD | BASE_MEM_PROT_CPU_WR |
                   BASE_MEM_PROT_GPU_RD | BASE_MEM_PROT_GPU_EX;

    printf("\n[kb v4] ========================================\n");
    printf("[kb v4] phase 7: confirming execution\n");
    printf("[kb v4] ========================================\n");
    printf("[kb v4] mem_alloc with GPU_EX (flags=0x%llx)...\n",
           (unsigned long long)mem.in.flags);

    int res = ioctl(dev->mali_fd, KBASE_IOCTL_MEM_ALLOC, &mem);
    if (res < 0) {
        printf("[kb v4] mem_alloc failed: %s\n", strerror(errno));
        return -1;
    }

    dev->queue_buffer_va = mem.out.gpu_va;
    printf("[kb v4] queue buffer at gpu va 0x%llx\n",
           (unsigned long long)dev->queue_buffer_va);

    /* ---- mmap queue buffer from CPU side ---- */
    size_t map_size = 8192;
    void *cpu_ptr = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                         dev->mali_fd, dev->queue_buffer_va);
    if (cpu_ptr == MAP_FAILED) {
        printf("[kb v4] mmap buffer failed: %s\n", strerror(errno));
        return -1;
    }
    printf("[kb v4] buffer mapped at %p\n", cpu_ptr);
    memset(cpu_ptr, 0, map_size);

    /* ---- write CSF program ---- */
    if (write_csf_instructions(cpu_ptr, map_size) < 0) {
        printf("[kb v4] failed writing instructions\n");
        munmap(cpu_ptr, map_size);
        return -1;
    }

    /* ---- register ---- */
    struct kbase_ioctl_cs_queue_register reg = {0};
    reg.buffer_gpu_addr = dev->queue_buffer_va;
    reg.buffer_size = 0x2000;
    reg.priority = 0;

    printf("[kb v4] cs_queue_register...\n");
    res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_REGISTER, &reg);
    if (res < 0) {
        printf("[kb v4] register failed: %s\n", strerror(errno));
        munmap(cpu_ptr, map_size);
        return -1;
    }
    printf("[kb v4] registered\n");

    /* ---- bind ---- */
    union kbase_ioctl_cs_queue_bind bind = {0};
    bind.in.buffer_gpu_addr = dev->queue_buffer_va;
    bind.in.group_handle = dev->group_handle;
    bind.in.csi_index = 0;

    printf("[kb v4] cs_queue_bind (group=%u)...\n", dev->group_handle);
    res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_BIND, &bind);
    if (res < 0) {
        printf("[kb v4] bind failed: %s\n", strerror(errno));
        munmap(cpu_ptr, map_size);
        return -1;
    }
    dev->queue_mmap_handle = bind.out.mmap_handle;
    printf("[kb v4] bound — mmap_handle 0x%llx\n",
           (unsigned long long)dev->queue_mmap_handle);

    /* ---- mmap user i/o pages (doorbell + input + output) ---- */
    size_t io_size = BASEP_QUEUE_NR_MMAP_USER_PAGES * PAGE_SIZE_4K;
    void *user_io_ptr = mmap(NULL, io_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                             dev->mali_fd, dev->queue_mmap_handle);
    if (user_io_ptr == MAP_FAILED) {
        printf("[kb v4] mmap user_io failed: %s\n", strerror(errno));
        printf("[kb v4] (r44 might handle this differently, continuing anyway)\n");
    } else {
        printf("[kb v4] user i/o pages mapped at %p (3 pages)\n", user_io_ptr);

        volatile __u32 *output_page = (volatile __u32 *)((char *)user_io_ptr + 2 * PAGE_SIZE_4K);
        printf("[kb v4] initial CS_ACTIVE = 0x%08x\n", output_page[CS_ACTIVE / 4]);
    }

    /* ---- kick ---- */
    struct kbase_ioctl_cs_queue_kick kick = {0};
    kick.buffer_gpu_addr = dev->queue_buffer_va;

    printf("[kb v4] cs_queue_kick (cmd 37)...\n");
    res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_KICK, &kick);
    if (res < 0) {
        printf("[kb v4] kick failed: %s\n", strerror(errno));
        munmap(cpu_ptr, map_size);
        if (user_io_ptr != MAP_FAILED) munmap(user_io_ptr, io_size);
        return -1;
    }
    printf("[kb v4] kick accepted\n");

    /* ---- wait for completion ---- */
    if (user_io_ptr != MAP_FAILED) {
        wait_for_completion(user_io_ptr, 2000);
        munmap(user_io_ptr, io_size);
    } else {
        printf("[kb v4] no i/o pages, sleeping 500ms...\n");
        usleep(500000);
    }

    /* ---- peek at buffer after kick ---- */
    __u64 *check = (__u64 *)cpu_ptr;
    printf("[kb v4] buffer after kick:\n");
    for (int i = 0; i < 5; i++) {
        printf("[kb v4]   [%d] = 0x%016llx\n", i, (unsigned long long)check[i]);
    }

    printf("[kb v4] ========================================\n");
    printf("[kb v4] phase 7 complete\n");
    printf("[kb v4] ========================================\n");

    munmap(cpu_ptr, map_size);
    return 0;
}
