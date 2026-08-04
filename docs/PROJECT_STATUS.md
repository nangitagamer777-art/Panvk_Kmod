# Project Status: Panvk_Kmod (Mali-G615 MC6)
Userspace reimplementation of the kbase ioctl protocol for the Dimensity 8300.

## My Test Environment
- Device: Poco X6 Pro (duchamp)
- SoC: MediaTek Dimensity 8300 (MT6897)
- GPU: Mali-G615 MC6 (Valhall architecture, CSF firmware)
- Kernel driver: mali_kbase_mt6897_r44 (MediaTek proprietary)
- Access node: /dev/mali0
- Environment: Native Termux with clang. Execution via Shizuku/rish session.
- GitHub: https://github.com/nangitagamer777-art/Panvk_Kmod

## Current File Structure
~/Panvk_Kmod/
├── include/
│   └── kbase_shim.h          # Structs, macros, kbase ioctls, CSF opcodes
├── src/
│   ├── main.c                 # Main test driver
│   ├── kbase_shim.c           # Init, context, generic memory (Phases 1-4)
│   ├── kbase_shim_v4.c        # GPU_EX buffer, register, bind, kick, I/O monitor (Phases 5-9)
│   └── kbase_shim_test_groups.c  # Group creation via cmd 42 (v1.6)
├── build/
│   └── shim_test_v4           # Current binary
├── docs/
│   └── PROJECT_STATUS.md      # This log
├── .gitignore
├── LICENSE                    # MIT
└── README.md

## What Works (Phases 1-9 Complete)

### Phase 1 — Handshake
VERSION_CHECK (cmd 52) with major=11, minor=11. Returns kbase 1.20.

### Phase 2 — Context
SET_FLAGS (cmd 1) + GET_CONTEXT_ID (cmd 17). Context created successfully.

### Phase 3 — Generic Memory
MEM_ALLOC (cmd 5). GPU VA 0x41000 with standard protection flags (0x200f).

### Phase 4 — Queue Group
QUEUE_GROUP_CREATE (cmd 58) with tiler/compute/fragment masks. Group handle 0.

### Phase 5 — Executable Buffer + Register + Bind
MEM_ALLOC with GPU_EX flag (0x17 = CPU_RD|CPU_WR|GPU_RD|GPU_EX). Kernel maps to EXEC_VA zone (0x800000001000). CS_QUEUE_REGISTER (cmd 36) with size 0x2000. CS_QUEUE_BIND (cmd 39) returns mmap_handle=0x30000.

### Phase 6 — Kick + Event Signal + Doorbell
CS_QUEUE_KICK (cmd 37) accepted. CS_EVENT_SIGNAL (cmd 44) accepted. Manual doorbell poke via user I/O page 0.

### Phase 7 — User I/O Page Mapping
mmap() on mmap_handle=0x30000 maps 3 firmware pages: doorbell + input (CS_INSERT) + output (CS_EXTRACT, CS_ACTIVE).

### Phase 8 — GPU Execution Confirmed
- 4 NOPs (32 bytes): EXTRACT baseline 0 → 32. All consumed.
- 32 NOPs (256 bytes): EXTRACT baseline 0 → 256. All consumed.
- 8 instructions (64 bytes): NOPs + MOVE32 + opcode 0x01. EXTRACT baseline 0 → 64. All consumed.

### Phase 9 — Opcode Discovery
- Opcodes 0x00 (NOP), 0x01, 0x02 (MOVE32) execute correctly.
- Opcodes 0x0F+ and 0x10+ are invalid or break bind.
- No END opcode needed — GPU executes until CS_INSERT is reached.
- Group creation via cmd 42 (v1.6) works better on MediaTek r44 than cmd 58.

## Key Discoveries
- GPU_EX (1<<4) is mandatory for CSF queue buffers. Without it, REGISTER fails with ENOENT.
- r44 kernel separates normal memory (0x41000) from executable (0x800000001000).
- CS_INSERT must be set before kick — tells the GPU how many bytes to consume.
- cmd 42 (QUEUE_GROUP_CREATE_1_6) activates the scheduler on r44; cmd 58 does not.
- No explicit END instruction needed — GPU runs until INSERT == EXTRACT.
- MOVE32 (opcode 0x02) works and can load immediate values into registers.
- Doorbell page (page 0 of user I/O) accepts writes to wake the scheduler.

## Pending

### High Priority
- Phase 10: PanVK integration — create pan_kmod shim backend.
- Map STORE_MULTIPLE and other compute opcodes from Mesa v12.xml.
- Test Vulkan shader execution through the shim.

### Medium Priority
- Scoreboard visibility: can we read MOVE32 results from I/O pages?
- STORE_MULTIPLE configuration for writing results to data buffers.
- Error handling: CS_QUEUE_TERMINATE (cmd 41), CS_QUEUE_GROUP_TERMINATE (cmd 43).

### Low Priority
- Multi-queue and priority support.
- Doorbell batching optimization.

## Quick Commands
Build: cd ~/Panvk_Kmod && clang -I./include -Wall -Wextra -o build/shim_test_v4 src/main.c src/kbase_shim.c src/kbase_shim_v4.c src/kbase_shim_test_groups.c && cp build/shim_test_v4 /sdcard/
Run (rish): cp /sdcard/shim_test_v4 /data/local/tmp/ && chmod 777 /data/local/tmp/shim_test_v4 && /data/local/tmp/shim_test_v4

## Technical Notes
Memory flags: CPU_RD=0x01 CPU_WR=0x02 GPU_RD=0x04 GPU_WR=0x08 GPU_EX=0x10
CSF opcodes: NOP=0x00 MOVE32=0x02 WAIT=0x05 (8 bytes, [63:56]=opcode)
I/O pages: Page 0=Doorbell, Page 1=Input (CS_INSERT), Page 2=Output (CS_EXTRACT + CS_ACTIVE)

---
*Updated August 4, 2026. Phases 1-9 complete. GPU execution confirmed at 256 bytes.*
