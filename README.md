# Panvk_Kmod — Userspace kbase shim for ARM Mali Valhall GPUs

Reimplementation of the kbase ioctl protocol in userspace to drive ARM Mali GPUs with CSF firmware without the proprietary driver stack.

## Supported GPUs (Valhall / CSF architecture)

| Generation | Models |
|-----------|--------|
| Gen 3 | Mali-G510, Mali-G610, Mali-G615, Mali-G710 |
| Gen 4 | Mali-G715, Mali-G720, Mali-G725, Mali-G620, Mali-G625 |
| Gen 5+ | Mali-G730, Mali-G820 (CSF-based, likely compatible) |

Primary test device: **Mali-G615 MC6** on MediaTek Dimensity 8300 (Poco X6 Pro).
Kernel driver: `mali_kbase_mt6897_r44`. Access node: `/dev/mali0`.

## What this is

A lightweight C shim that talks directly to `/dev/mali0` through the kbase ioctl interface. It handles the full CSF queue lifecycle: context creation, memory allocation (with GPU_EX for executable regions), queue group management, register/bind of command queues, scheduler kick, and I/O page monitoring for execution confirmation.

**End goal:** plug this into Mesa's `panvk` as a drop-in replacement for the kernel backend, giving Valhall GPUs an open-source Vulkan stack — no custom kernel, no blobs.

## Status

| Phase | What | Result |
|-------|------|--------|
| 1 | VERSION_CHECK (cmd 52) | Handshake OK, kbase 1.20 |
| 2 | SET_FLAGS (cmd 1) + GET_CONTEXT_ID (cmd 17) | Context created |
| 3 | MEM_ALLOC generic (cmd 5) | GPU VA 0x41000 |
| 4 | QUEUE_GROUP_CREATE (cmd 58) | Group handle 0 |
| 5 | MEM_ALLOC GPU_EX + REGISTER (36) + BIND (39) | Queue ready, mmap_handle 0x30000 |
| 6 | QUEUE_KICK (cmd 37) | Kick accepted |
| 7 | mmap user I/O pages + CS_ACTIVE poll | 3 pages mapped, monitoring works |
| 8 | 32 NOPs (256 bytes) executed | EXTRACT: 0 to 256, GPU confirmed |
| 9 | Opcode fuzzing | NOP (0x00), MOVE32 (0x02) work. No END needed. |
| 10 | PanVK integration | Next |

## Key Discoveries

- `GPU_EX (1<<4)` is mandatory for CSF queue buffers. Without it: ENOENT.
- r44 kernel separates normal memory (0x41000) from executable EXEC_VA (0x800000001000).
- `CS_INSERT` must be set before kick so the GPU knows how many bytes to consume.
- cmd 42 (QUEUE_GROUP_CREATE_1_6) activates the scheduler on r44; cmd 58 does not.
- No explicit END instruction needed. GPU runs until INSERT == EXTRACT.
- `MOVE32` (opcode 0x02) works for loading immediate values into registers.
- Doorbell page (page 0 of user I/O) accepts writes to wake the scheduler.

## Build (Termux + clang)

cd Panvk_Kmod
clang -I./include -Wall -Wextra -o build/shim_test_v4 src/main.c src/kbase_shim.c src/kbase_shim_v4.c src/kbase_shim_test_groups.c

## Run (Shizuku / rish)

cp build/shim_test_v4 /sdcard/
cp /sdcard/shim_test_v4 /data/local/tmp/
chmod 777 /data/local/tmp/shim_test_v4
/data/local/tmp/shim_test_v4

## Project Structure

Panvk_Kmod/
├── include/
│   └── kbase_shim.h          # Structs, macros, ioctls, CSF opcodes
├── src/
│   ├── main.c                 # Test driver
│   ├── kbase_shim.c           # Phases 1-4
│   ├── kbase_shim_v4.c        # Phases 5-9
│   └── kbase_shim_test_groups.c  # Group creation (cmd 42)
├── docs/
│   └── PROJECT_STATUS.md      # Full project log
├── .gitignore
├── LICENSE
└── README.md

## Credits

**Noin Haxel** (@nangitagamer777-art) — reverse engineering, implementation, testing.

## References

 * 0x36/Pixel_GPU_Exploit — ioctl command numbers
 * Google Project Zero, CVE-2023-4211 — kbase handshake pattern
 * Man Yue Mo, "Bypassing MTE with CVE-2025-0072" — CSF queue flow analysis
 * android.googlesource.com/kernel/google-modules/gpu — kbase kernel source

## License

MIT
