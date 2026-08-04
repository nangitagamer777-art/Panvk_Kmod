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

int kbase_shim_csf_kick_and_wait_v4(kbase_shim_device_t *dev) {
    if (!dev || dev->mali_fd < 0) return -1;

    /* Alloc queue buffer with GPU_EX */
    union kbase_ioctl_mem_alloc mem = {0};
    mem.in.va_pages = 2;
    mem.in.commit_pages = 2;
    mem.in.flags = BASE_MEM_PROT_CPU_RD | BASE_MEM_PROT_CPU_WR |
                   BASE_MEM_PROT_GPU_RD | BASE_MEM_PROT_GPU_EX;
    printf("\n[kb] alloc queue buffer (GPU_EX)...\n");
    if (ioctl(dev->mali_fd, KBASE_IOCTL_MEM_ALLOC, &mem) < 0) {
        printf("[kb] mem_alloc failed: %s\n", strerror(errno));
        return -1;
    }
    dev->queue_buffer_va = mem.out.gpu_va;
    printf("[kb] queue buffer at 0x%llx\n", (unsigned long long)dev->queue_buffer_va);

    /* mmap and write 4 NOPs */
    void *buf = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_SHARED,
                     dev->mali_fd, dev->queue_buffer_va);
    if (buf == MAP_FAILED) {
        printf("[kb] mmap failed: %s\n", strerror(errno));
        return -1;
    }
    memset(buf, 0, 8192);
    __u64 *instr = (__u64 *)buf;
    for (int i = 0; i < 4; i++) instr[i] = CSF_INSTR(CSF_OPCODE_NOP, 0);
    printf("[kb] wrote 4 NOPs (32 bytes)\n");

    /* Register */
    struct kbase_ioctl_cs_queue_register reg = {0};
    reg.buffer_gpu_addr = dev->queue_buffer_va;
    reg.buffer_size = 0x2000;
    reg.priority = 0;
    if (ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_REGISTER, &reg) < 0) {
        printf("[kb] register failed: %s\n", strerror(errno));
        munmap(buf, 8192);
        return -1;
    }
    printf("[kb] queue registered\n");

    /* Bind */
    union kbase_ioctl_cs_queue_bind bind = {0};
    bind.in.buffer_gpu_addr = dev->queue_buffer_va;
    bind.in.group_handle = dev->group_handle;
    bind.in.csi_index = 0;
    if (ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_BIND, &bind) < 0) {
        printf("[kb] bind failed: %s\n", strerror(errno));
        munmap(buf, 8192);
        return -1;
    }
    dev->queue_mmap_handle = bind.out.mmap_handle;
    printf("[kb] bound\n");

    /* mmap user I/O pages */
    size_t io_sz = BASEP_QUEUE_NR_MMAP_USER_PAGES * PAGE_SIZE_4K;
    void *io = mmap(NULL, io_sz, PROT_READ | PROT_WRITE, MAP_SHARED,
                    dev->mali_fd, dev->queue_mmap_handle);
    if (io == MAP_FAILED) {
        printf("[kb] user_io mmap failed: %s\n", strerror(errno));
        munmap(buf, 8192);
        return -1;
    }

    /* Baseline + set CS_INSERT */
    volatile __u32 *out = (volatile __u32 *)((char *)io + 2 * PAGE_SIZE_4K);
    volatile __u32 *inp = (volatile __u32 *)((char *)io + PAGE_SIZE_4K);
    printf("[kb] BASELINE EXTRACT = 0x%08x_%08x\n", out[CS_EXTRACT_HI/4], out[CS_EXTRACT_LO/4]);
    inp[CS_INSERT_LO/4] = 32;
    inp[CS_INSERT_HI/4] = 0;
    printf("[kb] CS_INSERT set to 32\n");

    /* Kick + event signal + doorbell */
    struct kbase_ioctl_cs_queue_kick kick = {0};
    kick.buffer_gpu_addr = dev->queue_buffer_va;
    ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_KICK, &kick);
    ioctl(dev->mali_fd, KBASE_IOCTL_CS_EVENT_SIGNAL);
    ((volatile __u32 *)io)[0] = 1;
    usleep(50000);
    ioctl(dev->mali_fd, KBASE_IOCTL_CS_QUEUE_KICK, &kick);
    printf("[kb] kicked + doorbell\n");

    /* Poll result */
    usleep(100000);
    __u32 extr_lo = out[CS_EXTRACT_LO/4];
    __u32 extr_hi = out[CS_EXTRACT_HI/4];
    __u32 active  = out[CS_ACTIVE/4];
    printf("[kb] POST-KICK EXTRACT = 0x%08x_%08x (%llu bytes) ACTIVE=0x%08x\n",
           extr_hi, extr_lo, (unsigned long long)extr_hi << 32 | extr_lo, active);

    if (extr_lo >= 32 || extr_hi != 0) {
        printf("[kb] >>> GPU EXECUTED ALL 32 BYTES! <<<\n");
    } else if (extr_lo > 0) {
        printf("[kb] GPU consumed %llu bytes (partial)\n",
               (unsigned long long)extr_hi << 32 | extr_lo);
    } else {
        printf("[kb] No execution detected\n");
    }

    munmap(io, io_sz);
    munmap(buf, 8192);
    return 0;
}
