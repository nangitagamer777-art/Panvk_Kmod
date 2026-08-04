# Panvk_Kmod - Userspace kbase shim for Mali-G615 MC6

Reimplementation of the kbase ioctl protocol in userspace to drive the
Mali-G615 MC6 GPU (MediaTek Dimensity 8300) without the proprietary
ARM driver stack.

## What this is

A lightweight C shim that talks directly to /dev/mali0 through the
kbase ioctl interface. It handles the full CSF queue lifecycle: context
creation, memory allocation (with GPU_EX for executable regions), queue
group management, register/bind of command queues, and scheduler kick
with I/O page monitoring.

End goal: plug this into Mesa panvk as a drop-in replacement for the
kernel backend, giving the Mali-G615 an open-source Vulkan stack with
no custom kernel and no blobs.

## Device

- Phone: Poco X6 Pro
- SoC: MediaTek Dimensity 8300 (MT6897)
- GPU: Mali-G615 MC6 (Valhall, CSF firmware)
- Kernel driver: mali_kbase_mt6897_r44
- Access node: /dev/mali0

## Build (Termux + clang)

    cd Panvk_Kmod
    clang -I./include -Wall -Wextra -o build/shim_test_v4 \
        src/main.c src/kbase_shim.c src/kbase_shim_v3.c src/kbase_shim_v4.c

## Run (Shizuku / rish)

    cp build/shim_test_v4 /sdcard/
    cp /sdcard/shim_test_v4 /data/local/tmp/
    chmod 777 /data/local/tmp/shim_test_v4
    /data/local/tmp/shim_test_v4

## Status

| Phase | IOCTL | Done |
|-------|-------|------|
| 1 | VERSION_CHECK (52) | yes |
| 2 | SET_FLAGS (1) | yes |
| 3 | MEM_ALLOC generic (5) | yes |
| 4 | QUEUE_GROUP_CREATE (58) | yes |
| 5 | MEM_ALLOC GPU_EX + REGISTER + BIND (5,36,39) | yes |
| 6 | QUEUE_KICK (37) | yes |
| 7 | mmap user_io + CS_ACTIVE poll | yes |
| 8 | STORE to result buffer | next |

## References

- 0x36/Pixel_GPU_Exploit
- Google Project Zero, CVE-2023-4211
- Man Yue Mo, Bypassing MTE with CVE-2025-0072
- android.googlesource.com/kernel/google-modules/gpu

## License

MIT
