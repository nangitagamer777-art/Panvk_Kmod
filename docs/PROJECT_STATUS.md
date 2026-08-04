# Project Status: Panvk_Kmod (Mali-G615 MC6)
Userspace reimplementation of the kbase ioctl protocol for the Dimensity 8300.

## My Test Environment
- Device: Poco X6 Pro (duchamp)
- SoC: MediaTek Dimensity 8300 (MT6897)
- GPU: Mali-G615 MC6 (Valhall architecture, CSF firmware)
- Kernel driver: mali_kbase_mt6897_r44 (MediaTek proprietary, not Google's)
- Access node: /dev/mali0 (crw-rw-rw-, major 10, minor 99)
- Environment: Native Termux with clang. Execution via Shizuku/rish session (u:r:shell:s0 context) to bypass node permissions.

## Current File Structure
~/Panvk_Kmod/
├── include/
│   └── kbase_shim.h          # Structs, macros, kbase ioctls, and CSF opcodes
├── src/
│   ├── main.c                 # Main test driver / orchestrator
│   ├── kbase_shim.c           # Init, context, generic memory (Phases 1-4)
│   ├── kbase_shim_v3.c        # Queue buffer with execution privileges (Phase 5)
│   └── kbase_shim_v4.c        # CSF instructions, kick, and I/O page monitor (Phases 6-7)
├── build/
│   └── shim_test_v4           # Currently running binary
├── docs/
│   └── PROJECT_STATUS.md      # This log
├── .gitignore
├── LICENSE                    # MIT
└── README.md                  # English documentation for GitHub

### Reference repos and sources:
- ~/mali-kbase-src/ -- kbase kernel source (Google Pixel branch, struct reference)
- Key kernel files: mali_kbase_csf_ioctl.h (CSF ioctls), mali_kbase_csf.c (register/bind/kick flow), mali_kbase_mem_linux.c (mmap I/O pages)
- External: 0x36/Pixel_GPU_Exploit for ioctl numbers, Man Yue Mo CVE-2025-0072 write-up
- Mesa environment: ~/mesa-26.2.0-rc2/ (panvk build in Debian proot for future testing)
- GitHub repo: https://github.com/nangitagamer777-art/Panvk_Kmod

## What I've Got Working (Phases 1-7)

### Phase 1 -- Initial Handshake
Ran KBASE_IOCTL_VERSION_CHECK (cmd 52) with major=11, minor=11. Clean handshake, kbase version 1.20.

### Phase 2 -- Context Creation
KBASE_IOCTL_SET_FLAGS (cmd 1) with flags=0 went clean. Got real context ID via KBASE_IOCTL_GET_CONTEXT_ID (cmd 17).

### Phase 3 -- Generic Memory
KBASE_IOCTL_MEM_ALLOC (cmd 5), got GPU VA 0x41000 with output flags 0x200f.

### Phase 4 -- Queue Group
KBASE_IOCTL_CS_QUEUE_GROUP_CREATE (cmd 58) with tiler/compute/fragment masks. Got group_handle=0.

### Phase 5 -- Executable Buffer, Register and Bind
MEM_ALLOC with GPU_EX flag (0x17 = CPU_RD|CPU_WR|GPU_RD|GPU_EX). Kernel mapped it to EXEC_VA zone (0x800000001000). CS_QUEUE_REGISTER (cmd 36) with size 0x2000, prio 0. CS_QUEUE_BIND (cmd 39) on csi_index=0 returned mmap_handle=0x30000.

### Phase 6 -- Instruction Injection and CSF Kick
Wrote 5 CSF instructions (NOP, MOVE32 0xDEAD, WAIT, NOP, END, 40 bytes) into CPU-mapped buffer. KBASE_IOCTL_CS_QUEUE_KICK (cmd 37) accepted. First kick accepted by the CSF scheduler!

### Phase 7 -- I/O Page Mapping and Execution Monitoring
mmap() on mmap_handle=0x30000 mapped 3 firmware control pages (doorbell + input + output). 10ms polling loop on CS_ACTIVE register from output page. Hardware processed the batch before the first loop cycle finished (CS_ACTIVE=0). Full CSF queue lifecycle control from userspace!

### Loaded instruction batch (V4.1):
[0] NOP           = 0x0000000000000000
[1] MOVE32 0xDEAD = 0x02000000dead0000
[2] WAIT 0x100    = 0x0500000000000100
[3] NOP           = 0x0000000000000000
[4] END           = 0x0a00000000000000
Total: 5 instructions (40 bytes)

## Key Discoveries
- GPU_EX (1<<4) is mandatory for CSF queue buffers. Without it, REGISTER fails with ENOENT.
- r44 kernel separates normal memory (0x41000) from executable (0x800000001000).
- Bind mmap_handle gives 3 contiguous I/O pages. CS_ACTIVE is at output_page[2], bit 0 = queue active.
- CS_QUEUE_KICK (cmd 37) only needs the buffer address. Kernel accepts it cleanly.
- All code cleaned up in English for GitHub. Spanish docs archived separately.

## What's Coming

### High Priority
- Phase 8: STORE instruction to write 0xDEAD into a separate data buffer for real execution proof.
- Phase 9: CS_EVENT_SIGNAL (cmd 44) or doorbell interrupt mapping for notification without polling.

### Medium Priority
- Opcode verification against Mesa v12.xml for STORE, CALL, COND_BRANCH.
- Fault handling: CS_QUEUE_TERMINATE (cmd 41) and CS_QUEUE_GROUP_TERMINATE (cmd 43).

### Low Priority
- PanVK integration as pan_kmod backend replacement.
- Doorbell batching and multi-queue priority support.

## Quick Commands
Build: clang -I./include -Wall -Wextra -o build/shim_test_v4 src/main.c src/kbase_shim.c src/kbase_shim_v3.c src/kbase_shim_v4.c
Run: cp build/shim_test_v4 /sdcard/ && cp /sdcard/shim_test_v4 /data/local/tmp/ && chmod 777 /data/local/tmp/shim_test_v4 && /data/local/tmp/shim_test_v4
Debug: strace -e ioctl /data/local/tmp/shim_test_v4 2>&1

## Technical Notes
Memory flags: CPU_RD=0x01 CPU_WR=0x02 GPU_RD=0x04 GPU_WR=0x08 GPU_EX=0x10 (mandatory for ring buffers)
CSF opcodes (8 bytes, [63:56]=opcode): NOP=0x00 MOVE32=0x02 WAIT=0x05 END=0x0A CALL=0x0B COND_BRANCH=0x0C
I/O pages (3): Page 0=Doorbell, Page 1=Input (CS_INSERT), Page 2=Output (CS_EXTRACT + CS_ACTIVE)

---
*Log updated August 3, 2026. Phases 1-7 complete. Code on GitHub.*
