#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define KBASE_IOCTL_TYPE  0x80
#define BASE_MEM_PROT_CPU_RD  (1U << 0)
#define BASE_MEM_PROT_CPU_WR  (1U << 1)
#define BASE_MEM_PROT_GPU_RD  (1U << 2)
#define BASE_MEM_PROT_GPU_WR  (1U << 3)
#define BASE_MEM_PROT_GPU_EX  (1U << 4)
#define CSF_OPCODE_NOP  0x00
#define CSF_INSTR(op, arg) (((__u64)(op) << 56) | ((arg) & 0x00FFFFFFFFFFFFFFULL))

struct kbase_ioctl_version_check { __u16 major; __u16 minor; };
#define KBASE_IOCTL_VERSION_CHECK _IOWR(0x80, 52, struct kbase_ioctl_version_check)

struct kbase_ioctl_set_flags { __u32 create_flags; };
#define KBASE_IOCTL_SET_FLAGS _IOW(0x80, 1, struct kbase_ioctl_set_flags)

union kbase_ioctl_mem_alloc {
    struct { __u64 va_pages; __u64 commit_pages; __u64 extension; __u64 flags; } in;
    struct { __u64 flags; __u64 gpu_va; } out;
};
#define KBASE_IOCTL_MEM_ALLOC _IOWR(0x80, 5, union kbase_ioctl_mem_alloc)

struct kbase_ioctl_cs_queue_register {
    __u64 buffer_gpu_addr; __u32 buffer_size; __u8 priority; __u8 padding[3];
};
#define KBASE_IOCTL_CS_QUEUE_REGISTER _IOW(0x80, 36, struct kbase_ioctl_cs_queue_register)

struct kbase_ioctl_cs_queue_kick { __u64 buffer_gpu_addr; };
#define KBASE_IOCTL_CS_QUEUE_KICK _IOW(0x80, 37, struct kbase_ioctl_cs_queue_kick)

union kbase_ioctl_cs_queue_bind {
    struct { __u64 buffer_gpu_addr; __u8 group_handle; __u8 csi_index; __u8 padding[6]; } in;
    struct { __u64 mmap_handle; } out;
};
#define KBASE_IOCTL_CS_QUEUE_BIND _IOWR(0x80, 39, union kbase_ioctl_cs_queue_bind)

union kbase_ioctl_cs_queue_group_create_1_6 {
    struct { __u64 tiler_mask; __u64 fragment_mask; __u64 compute_mask;
             __u8 cs_min; __u8 priority; __u8 tiler_max; __u8 fragment_max;
             __u8 compute_max; __u8 padding[3]; } in;
    struct { __u8 group_handle; __u8 padding[3]; __u32 group_uid; } out;
};
#define KBASE_IOCTL_CS_QUEUE_GROUP_CREATE_1_6 _IOWR(0x80, 42, union kbase_ioctl_cs_queue_group_create_1_6)
#define KBASE_IOCTL_CS_EVENT_SIGNAL _IO(0x80, 44)

#define USER_IO_PAGES 3
#define PAGE_SZ 4096
#define CS_INSERT_LO  0x0000
#define CS_INSERT_HI  0x0004
#define CS_EXTRACT_LO 0x0000
#define CS_EXTRACT_HI 0x0004

struct kbase_dev { int fd; uint8_t group_handle; };
struct kbase_bo { uint64_t gpu_va; size_t size; void *cpu_map; };
struct kbase_queue { uint64_t ring_va; uint64_t mmap_handle; void *user_io; uint32_t ring_size; };

static int kbase_dev_open(void) {
    int fd = open("/dev/mali0", O_RDWR);
    if (fd < 0) return -1;
    struct kbase_ioctl_version_check ver = { .major = 11, .minor = 11 };
    if (ioctl(fd, KBASE_IOCTL_VERSION_CHECK, &ver) < 0) { close(fd); return -1; }
    struct kbase_ioctl_set_flags flags = { .create_flags = 0 };
    if (ioctl(fd, KBASE_IOCTL_SET_FLAGS, &flags) < 0) { close(fd); return -1; }
    return fd;
}

static struct kbase_dev *kbase_dev_create(void) {
    struct kbase_dev *dev = calloc(1, sizeof(*dev));
    if (!dev) return NULL;
    dev->fd = kbase_dev_open();
    if (dev->fd < 0) { free(dev); return NULL; }
    union kbase_ioctl_cs_queue_group_create_1_6 grp = {0};
    grp.in.tiler_mask = ~0ULL; grp.in.fragment_mask = ~0ULL; grp.in.compute_mask = ~0ULL;
    grp.in.cs_min = 1; grp.in.tiler_max = 8; grp.in.fragment_max = 8; grp.in.compute_max = 8;
    if (ioctl(dev->fd, KBASE_IOCTL_CS_QUEUE_GROUP_CREATE_1_6, &grp) < 0) { close(dev->fd); free(dev); return NULL; }
    dev->group_handle = grp.out.group_handle;
    return dev;
}

static void kbase_dev_destroy(struct kbase_dev *dev) {
    if (dev) { close(dev->fd); free(dev); }
}

static struct kbase_bo *kbase_bo_alloc(struct kbase_dev *dev, size_t size, int executable) {
    struct kbase_bo *bo = calloc(1, sizeof(*bo));
    if (!bo) return NULL;
    union kbase_ioctl_mem_alloc mem = {0};
    mem.in.va_pages = (size + 4095) / 4096;
    mem.in.commit_pages = mem.in.va_pages;
    mem.in.flags = BASE_MEM_PROT_CPU_RD | BASE_MEM_PROT_CPU_WR | BASE_MEM_PROT_GPU_RD;
    mem.in.flags |= executable ? BASE_MEM_PROT_GPU_EX : BASE_MEM_PROT_GPU_WR;
    if (ioctl(dev->fd, KBASE_IOCTL_MEM_ALLOC, &mem) < 0) { free(bo); return NULL; }
    bo->gpu_va = mem.out.gpu_va;
    bo->size = size;
    bo->cpu_map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, bo->gpu_va);
    if (bo->cpu_map == MAP_FAILED) bo->cpu_map = NULL;
    return bo;
}

static void kbase_bo_free(struct kbase_bo *bo) {
    if (bo) { if (bo->cpu_map) munmap(bo->cpu_map, bo->size); free(bo); }
}

static struct kbase_queue *kbase_queue_create(struct kbase_dev *dev, struct kbase_bo *ring_bo) {
    struct kbase_queue *q = calloc(1, sizeof(*q));
    if (!q) return NULL;
    q->ring_va = ring_bo->gpu_va;
    q->ring_size = ring_bo->size;
    struct kbase_ioctl_cs_queue_register reg = {0};
    reg.buffer_gpu_addr = q->ring_va; reg.buffer_size = q->ring_size; reg.priority = 0;
    if (ioctl(dev->fd, KBASE_IOCTL_CS_QUEUE_REGISTER, &reg) < 0) { free(q); return NULL; }
    union kbase_ioctl_cs_queue_bind bind = {0};
    bind.in.buffer_gpu_addr = q->ring_va; bind.in.group_handle = dev->group_handle; bind.in.csi_index = 0;
    if (ioctl(dev->fd, KBASE_IOCTL_CS_QUEUE_BIND, &bind) < 0) { free(q); return NULL; }
    q->mmap_handle = bind.out.mmap_handle;
    q->user_io = mmap(NULL, USER_IO_PAGES * PAGE_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, q->mmap_handle);
    if (q->user_io == MAP_FAILED) q->user_io = NULL;
    return q;
}

static void kbase_queue_destroy(struct kbase_queue *q) {
    if (q) { if (q->user_io) munmap(q->user_io, USER_IO_PAGES * PAGE_SZ); free(q); }
}

static int kbase_queue_submit(struct kbase_dev *dev, struct kbase_queue *q, __u64 *instrs, int num) {
    if (!q->user_io) return -1;
    int bytes = num * 8;
    void *ring = mmap(NULL, q->ring_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, q->ring_va);
    if (ring == MAP_FAILED) return -1;
    memcpy(ring, instrs, bytes);
    volatile __u32 *inp = (volatile __u32 *)((char *)q->user_io + PAGE_SZ);
    inp[CS_INSERT_LO / 4] = bytes;
    inp[CS_INSERT_HI / 4] = 0;
    struct kbase_ioctl_cs_queue_kick kick = { .buffer_gpu_addr = q->ring_va };
    ioctl(dev->fd, KBASE_IOCTL_CS_QUEUE_KICK, &kick);
    ioctl(dev->fd, KBASE_IOCTL_CS_EVENT_SIGNAL);
    ((volatile __u32 *)q->user_io)[0] = 1;
    usleep(50000);
    ioctl(dev->fd, KBASE_IOCTL_CS_QUEUE_KICK, &kick);
    munmap(ring, q->ring_size);
    return 0;
}

int main(void) {
    printf("kbase_kmod test\n");
    struct kbase_dev *dev = kbase_dev_create();
    if (!dev) { printf("dev create failed\n"); return 1; }
    printf("device ready, group=%u\n", dev->group_handle);
    struct kbase_bo *ring = kbase_bo_alloc(dev, 8192, 1);
    if (!ring) { printf("ring alloc failed\n"); kbase_dev_destroy(dev); return 1; }
    printf("ring at 0x%llx\n", (unsigned long long)ring->gpu_va);
    struct kbase_queue *q = kbase_queue_create(dev, ring);
    if (!q) { printf("queue create failed\n"); kbase_bo_free(ring); kbase_dev_destroy(dev); return 1; }
    printf("queue ready, mmap=0x%llx\n", (unsigned long long)q->mmap_handle);
    __u64 instrs[4];
    for (int i = 0; i < 4; i++) instrs[i] = CSF_INSTR(CSF_OPCODE_NOP, 0);
    if (kbase_queue_submit(dev, q, instrs, 4) == 0) {
        usleep(100000);
        volatile __u32 *out = (volatile __u32 *)((char *)q->user_io + 2 * PAGE_SZ);
        __u32 lo = out[CS_EXTRACT_LO / 4], hi = out[CS_EXTRACT_HI / 4];
        printf("EXTRACT = 0x%08x_%08x (%llu bytes)\n", hi, lo, (unsigned long long)hi << 32 | lo);
        if (lo >= 32) printf(">>> ALL 32 BYTES EXECUTED <<<\n");
    }
    kbase_queue_destroy(q);
    kbase_bo_free(ring);
    kbase_dev_destroy(dev);
    return 0;
}
