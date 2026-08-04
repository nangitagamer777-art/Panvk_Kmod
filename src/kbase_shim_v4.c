#include "kbase_shim.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define BASEP_QUEUE_NR_MMAP_USER_PAGES 3
#define PAGE_SIZE_4K 4096
#define CS_INSERT_LO  0x0000
#define CS_INSERT_HI  0x0004
#define CS_EXTRACT_LO 0x0000
#define CS_EXTRACT_HI 0x0004
#define CS_ACTIVE      0x0008

#ifndef KBASE_IOCTL_CS_EVENT_SIGNAL
#define KBASE_IOCTL_CS_EVENT_SIGNAL _IO(0x80, 44)
#endif

static int write_csf_instructions(void *cpu_ptr, size_t buffer_size) {
    if (!cpu_ptr || buffer_size < 32) return -1;
    __u64 *instr = (__u64 *)cpu_ptr;
    for (int i = 0; i < 4; i++) {
        instr[i] = CSF_INSTR(CSF_OPCODE_NOP, 0);
        printf("[csf]   [%d] NOP = 0x%016llx\n", i, (unsigned long long)instr[i]);
    }
    printf("[csf] wrote 4 NOPs (32 bytes), no END\n");
    return 32;
}

static int wait_for_completion(void *user_io_ptr, int timeout_ms) {
    volatile __u32 *output_page = (volatile __u32 *)((char *)user_io_ptr + 2 * PAGE_SIZE_4K);
    int waited = 0;
    __u32 last_extr = 0;
    printf("[csf] polling (timeout %dms)...\n", timeout_ms);
    while (waited < timeout_ms) {
        __u32 active  = output_page[CS_ACTIVE / 4];
        __u32 extr_lo = output_page[CS_EXTRACT_LO / 4];
        __u32 extr_hi = output_page[CS_EXTRACT_HI / 4];
        if (extr_lo != last_extr || waited < 20 || waited % 500 == 0) {
            printf("[csf]   t=%dms ACTIVE=0x%08x EXTRACT=0x%08x_%08x (%llu bytes)\n",
                   waited, active, extr_hi, extr_lo,
                   (unsigned long long)extr_hi << 32 | extr_lo);
            last_extr = extr_lo;
        }
        if (extr_lo >= 32 || extr_hi != 0) {
            printf("[csf] >>> EXTRACT advanced past all NOPs! <<<\n");
            return 0;
        }
        if (!(active & 0x1) && waited > 100 && extr_lo == 8) {
            printf("[csf] stuck at 8 bytes, END is definitely the blocker\n");
            return 0;
        }
        usleep(10000);
        waited += 10;
    }
    printf("[csf] timeout\n");
    return -1;
}

int kbase_shim_csf_kick_and_wait_v4(kbase_shim_device_t *dev) {
    if (!dev || dev->mali_fd < 0) return -1;
    union kbase_ioctl_mem_alloc mem = {0};
    mem.in.va_pages = 2;
    mem.in.commit_pages = 2;
    mem.in.extension = 0;
    mem.in.flags = BASE_MEM_PROT_CPU_RD | BASE_MEM_PROT_CPU_WR |
                   BASE_MEM_PROT_GPU_RD | BASE_MEM_PROT_GPU_EX;
    printf("\n[kb v4] === 4xNOP, no END ===\n");
    int res = ioctl(dev->mali_fd, KBASE_IOCTL_MEM_ALLOC, &mem);
    if (res < 0) { printf("[kb v4] mem_alloc failed\n"); return -1; }
    dev->queue_buffer_va = mem.out.gpu_va;
    printf("[kb v4] gpu va 0x%llx\n", (unsigned long long)dev->queue_buffer_va);
    void *cpu_ptr = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, dev->mali_fd, dev->queue_buffer_va);
    if (cpu_ptr == MAP_FAILED) { printf("[kb v4] mmap failed\n"); return -1; }
    memset(cpu_ptr, 0, 8192);
    int bytes = write_csf_instructions(cpu_ptr, 8192);
    struct kbase_ioctl_cs_queue_register reg = {0};
    reg.buffer_gpu_addr = dev->queue_buffer_va;
    reg.buffer_size = 0x2000;
    reg.priority = 0;
    res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_REGISTER, &reg);
    if (res < 0) { printf("[kb v4] register failed\n"); return -1; }
    union kbase_ioctl_cs_queue_bind bind = {0};
    bind.in.buffer_gpu_addr = dev->queue_buffer_va;
    bind.in.group_handle = dev->group_handle;
    bind.in.csi_index = 0;
    res = ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_BIND, &bind);
    if (res < 0) { printf("[kb v4] bind failed\n"); return -1; }
    dev->queue_mmap_handle = bind.out.mmap_handle;
    size_t io_size = BASEP_QUEUE_NR_MMAP_USER_PAGES * PAGE_SIZE_4K;
    void *user_io_ptr = mmap(NULL, io_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev->mali_fd, dev->queue_mmap_handle);
    if (user_io_ptr != MAP_FAILED) {
        volatile __u32 *output_page = (volatile __u32 *)((char *)user_io_ptr + 2 * PAGE_SIZE_4K);
        printf("[kb v4] BASELINE: EXTRACT=0x%08x_%08x\n",
               output_page[CS_EXTRACT_HI/4], output_page[CS_EXTRACT_LO/4]);
        volatile __u32 *input_page = (volatile __u32 *)((char *)user_io_ptr + PAGE_SIZE_4K);
        input_page[CS_INSERT_LO / 4] = bytes;
        input_page[CS_INSERT_HI / 4] = 0;
        printf("[kb v4] CS_INSERT = %d\n", bytes);
    }
    struct kbase_ioctl_cs_queue_kick kick = {0};
    kick.buffer_gpu_addr = dev->queue_buffer_va;
    ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_KICK, &kick);
    ioctl(dev->mali_fd, KBASE_IOCTL_CS_EVENT_SIGNAL);
    if (user_io_ptr != MAP_FAILED) {
        volatile __u32 *db = (volatile __u32 *)user_io_ptr;
        db[0] = 1;
        usleep(50000);
        ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_KICK, &kick);
        wait_for_completion(user_io_ptr, 2000);
        munmap(user_io_ptr, io_size);
    }
    munmap(cpu_ptr, 8192);
    return 0;
}
