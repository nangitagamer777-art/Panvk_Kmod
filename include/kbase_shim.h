#ifndef KBASE_SHIM_H
#define KBASE_SHIM_H

#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/types.h>

/* --- 1. COMANDOS BASE KBASE IOCTL (ARM MALI, arquitectura CSF/Valhall) ---
 * Numeros de comando confirmados reales, extraidos de mali.h
 * (0x36/Pixel_GPU_Exploit, exploit real y moderno para GPUs Pixel CSF).
 */
#define KBASE_IOCTL_TYPE 0x80

#define BASE_MEM_PROT_CPU_RD (1U << 0)
#define BASE_MEM_PROT_CPU_WR (1U << 1)
#define BASE_MEM_PROT_GPU_RD (1U << 2)
#define BASE_MEM_PROT_GPU_WR (1U << 3)
#define BASE_MEM_PROT_GPU_EX (1U << 4)

struct kbase_ioctl_version_check {
    __u16 major;
    __u16 minor;
};
#define KBASE_IOCTL_VERSION_CHECK \
    _IOWR(KBASE_IOCTL_TYPE, 52, struct kbase_ioctl_version_check)

struct kbase_ioctl_set_flags {
    __u32 create_flags;
};
#define KBASE_IOCTL_SET_FLAGS \
    _IOW(KBASE_IOCTL_TYPE, 1, struct kbase_ioctl_set_flags)

struct kbase_ioctl_get_context_id {
    __u32 id;
};
#define KBASE_IOCTL_GET_CONTEXT_ID \
    _IOR(KBASE_IOCTL_TYPE, 17, struct kbase_ioctl_get_context_id)

union kbase_ioctl_mem_alloc {
    struct {
        __u64 va_pages;
        __u64 commit_pages;
        __u64 extension;
        __u64 flags;
    } in;
    struct {
        __u64 flags;
        __u64 gpu_va;
    } out;
};
#define KBASE_IOCTL_MEM_ALLOC \
    _IOWR(KBASE_IOCTL_TYPE, 5, union kbase_ioctl_mem_alloc)

struct kbase_ioctl_cs_queue_register {
    __u64 buffer_gpu_addr;
    __u32 buffer_size;
    __u8 priority;
    __u8 padding[3];
};
#define KBASE_IOCTL_CS_QUEUE_REGISTER \
    _IOW(KBASE_IOCTL_TYPE, 36, struct kbase_ioctl_cs_queue_register)

union kbase_ioctl_cs_queue_bind {
    struct {
        __u64 buffer_gpu_addr;
        __u8 group_handle;
        __u8 csi_index;
        __u8 padding[6];
    } in;
    struct {
        __u64 mmap_handle;
    } out;
};
#define KBASE_IOCTL_CS_QUEUE_BIND \
    _IOWR(KBASE_IOCTL_TYPE, 39, union kbase_ioctl_cs_queue_bind)

union kbase_ioctl_cs_queue_group_create {
    struct {
        __u64 tiler_mask;
        __u64 fragment_mask;
        __u64 compute_mask;
        __u8 cs_min;
        __u8 priority;
        __u8 tiler_max;
        __u8 fragment_max;
        __u8 compute_max;
        __u8 csi_handlers;
        __u8 padding[2];
        __u64 reserved;
    } in;
    struct {
        __u8 group_handle;
        __u8 padding[3];
        __u32 group_uid;
    } out;
};
#define KBASE_IOCTL_CS_QUEUE_GROUP_CREATE \
    _IOWR(KBASE_IOCTL_TYPE, 58, union kbase_ioctl_cs_queue_group_create)

/* --- 2. ESTRUCTURA DE TRADUCCION SHIM --- */
typedef struct {
    int mali_fd;
    uint32_t gpu_id;
    uint32_t context_id;
    uint16_t kbase_major;
    uint16_t kbase_minor;
    uint8_t group_handle;
    uint64_t queue_buffer_va;
    uint64_t queue_mmap_handle;
} kbase_shim_device_t;

/* --- 3. DECLARACION DE FUNCIONES SHIM --- */
int kbase_shim_init(kbase_shim_device_t *dev);
int kbase_shim_get_gpu_props(kbase_shim_device_t *dev);
int kbase_shim_create_group(kbase_shim_device_t *dev);
int kbase_shim_register_and_bind_queue(kbase_shim_device_t *dev);
void kbase_shim_close(kbase_shim_device_t *dev);

#endif /* KBASE_SHIM_H */

/* Flags de protección de memoria para la GPU */
#ifndef BASE_MEM_PROT_GPU_RD
#define BASE_MEM_PROT_GPU_RD  (1ULL << 0)
#define BASE_MEM_PROT_GPU_WR  (1ULL << 1)
#endif

/* --- FUNCIONES V2 (INYECCION PARA EVITAR ENOENT) --- */
int kbase_shim_register_and_bind_queue_v2(kbase_shim_device_t *dev);

/* V3: register + bind con GPU_EX */
int kbase_shim_register_and_bind_queue_v3(kbase_shim_device_t *dev);

/* --- IOCTL: CS_QUEUE_KICK (cmd 37) --- */
struct kbase_ioctl_cs_queue_kick {
    __u64 buffer_gpu_addr;
};

#define KBASE_IOCTL_CS_QUEUE_KICK \
    _IOW(KBASE_IOCTL_TYPE, 37, struct kbase_ioctl_cs_queue_kick)

/* --- IOCTL: CS_QUEUE_TERMINATE (cmd 41) --- */
struct kbase_ioctl_cs_queue_terminate {
    __u64 buffer_gpu_addr;
};

#define KBASE_IOCTL_CS_QUEUE_TERMINATE \
    _IOW(KBASE_IOCTL_TYPE, 41, struct kbase_ioctl_cs_queue_terminate)

/* --- Instrucciones CSF básicas (Valhall) --- */
/* Cada instrucción CSF son 8 bytes. Formato:
 * bits [63:56] = opcode
 * bits [55:0]  = argumentos (depende del opcode)
 */
#define CSF_OPCODE_NOP          0x00
#define CSF_OPCODE_MOVE32       0x02
#define CSF_OPCODE_WAIT         0x05
#define CSF_OPCODE_END          0x0A
#define CSF_OPCODE_CALL         0x0B
#define CSF_OPCODE_COND_BRANCH  0x0C

/* Macro para construir una instrucción CSF */
#define CSF_INSTR(opcode, arg)  (((__u64)(opcode) << 56) | ((arg) & 0x00FFFFFFFFFFFFFFULL))

/* V4: register + bind + instrucciones CSF + kick + wait */
int kbase_shim_csf_kick_and_wait_v4(kbase_shim_device_t *dev);
