# Panvk_Kmod — Userspace kbase shim for ARM Mali Valhall GPUs

Reimplementation of the kbase ioctl protocol in userspace to drive ARM Mali GPUs with CSF firmware without the proprietary driver stack.

## Supported GPUs (Valhall / CSF architecture)

| Generation | Models |
|-----------|--------|
| Gen 3 | Mali-G510, Mali-G610, Mali-G615, Mali-G710 |
| Gen 4 | Mali-G715, Mali-G720, Mali-G725, Mali-G620, Mali-G625 |
| Gen 5+ | Mali-G730, Mali-G820 (CSF-based, likely compatible) |

Primary test device: **Mali-G615 MC6** on MediaTek Dimensity 8300 (Poco X6 Pro). Kernel driver: `mali_kbase_mt6897_r44`.

## Status

| Phase | What | Result |
|-------|------|--------|
| 1 | VERSION_CHECK (cmd 52) | Handshake OK, kbase 1.20 |
| 2 | SET_FLAGS (cmd 1) + GET_CONTEXT_ID (cmd 17) | Context created |
| 3 | MEM_ALLOC generic (cmd 5) | GPU VA 0x41000 |
| 4 | QUEUE_GROUP_CREATE (cmd 58) | Group handle 0 |
| 5 | MEM_ALLOC GPU_EX + REGISTER (36) + BIND (39) | Queue ready, mmap_handle 0x30000 |
| 6 | QUEUE_KICK (cmd 37) | Kick accepted |
| 7 | mmap user I/O pages + CS_ACTIVE poll | 3 pages mapped |
| 8 | 32 NOPs (256 bytes) executed | EXTRACT: 0 to 256 |
| 9 | Opcode fuzzing | NOP, MOVE32 work. No END needed. |
| 10 | PanVK integration | Next |

## Build

cd Panvk_Kmod && clang -I./include -Wall -Wextra -o build/shim_test_v4 src/main.c src/kbase_shim.c src/kbase_shim_v4.c src/kbase_shim_test_groups.c

## Run

cp build/shim_test_v4 /sdcard/ && cp /sdcard/shim_test_v4 /data/local/tmp/ && chmod 777 /data/local/tmp/shim_test_v4 && /data/local/tmp/shim_test_v4

## Credits

**Noin Haxel** ([@nangitagamer777-art](https://github.com/nangitagamer777-art))

## License

MIT
