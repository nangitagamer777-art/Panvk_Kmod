# Panvk_Kmod - Userspace kbase shim for ARM Mali Valhall GPUs
Reimplementation of the kbase ioctl protocol in userspace to drive ARM Mali GPUs with CSF firmware without the proprietary driver stack.

## Supported GPUs (Valhall / CSF architecture)
Gen 3: Mali-G510, Mali-G610, Mali-G615, Mali-G710
Gen 4: Mali-G715, Mali-G720, Mali-G725, Mali-G620, Mali-G625
Gen 5+: Mali-G730, Mali-G820 (CSF-based, likely compatible)
Primary test device: Mali-G615 MC6 on MediaTek Dimensity 8300 (Poco X6 Pro). Kernel driver mali_kbase_mt6897_r44.

## Status
| Phase | IOCTL | Done |
|-------|-------|------|
| 1 | VERSION_CHECK (52) | yes |
| 2 | SET_FLAGS (1) + GET_CONTEXT_ID (17) | yes |
| 3 | MEM_ALLOC generic (5) | yes |
| 4 | QUEUE_GROUP_CREATE (58) | yes |
| 5 | MEM_ALLOC GPU_EX + REGISTER + BIND (5,36,39) | yes |
| 6 | QUEUE_KICK (37) | yes |
| 7 | mmap user_io + CS_ACTIVE poll | yes |
| 8 | GPU execution: 32 NOPs (256 bytes) confirmed | yes |
| 9 | Opcode discovery: MOVE32 works, no END needed | yes |
| 10 | PanVK integration | next |

## Build (Termux + clang)
cd Panvk_Kmod && clang -I./include -Wall -Wextra -o build/shim_test_v4 src/main.c src/kbase_shim.c src/kbase_shim_v4.c src/kbase_shim_test_groups.c

## Run (Shizuku / rish)
cp build/shim_test_v4 /sdcard/ && cp /sdcard/shim_test_v4 /data/local/tmp/ && chmod 777 /data/local/tmp/shim_test_v4 && /data/local/tmp/shim_test_v4

## Credits
Noin Haxel (@nangitagamer777-art) - reverse engineering, implementation, testing.

## References
0x36/Pixel_GPU_Exploit, Google Project Zero CVE-2023-4211, Man Yue Mo CVE-2025-0072, android.googlesource.com/kernel/google-modules/gpu

## License
MIT
