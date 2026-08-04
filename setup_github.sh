#!/bin/bash
set -e

echo "=== Creando .gitignore ==="
cat > .gitignore << 'EOF'
build/
*.o
*.swp
*.bak
.vscode/
.DS_Store
EOF

echo "=== Creando README.md ==="
cat > README.md << 'EOF'
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
EOF

echo "=== Creando LICENSE ==="
cat > LICENSE << 'EOF'
MIT License

Copyright (c) 2026 Noin Haxel

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
EOF

echo "=== Creando docs/PROJECT_STATUS.md ==="
mkdir -p docs
cat > docs/PROJECT_STATUS.md << 'EOF'
# Estado del Proyecto: Panvk_Kmod (Mali-G615 MC6)

Reimplementacion del protocolo kbase en espacio de usuario
para el Dimensity 8300.

## Entorno

- Dispositivo: Poco X6 Pro (duchamp)
- SoC: MediaTek Dimensity 8300 (MT6897)
- GPU: Mali-G615 MC6 (Valhall, firmware CSF)
- Driver: mali_kbase_mt6897_r44 (MediaTek)
- Nodo: /dev/mali0
- Entorno: Termux + clang, ejecucion via Shizuku/rish

## Fases completadas (1 a 7)

1. VERSION_CHECK (cmd 52) -> handshake kbase 1.20
2. SET_FLAGS (cmd 1) + GET_CONTEXT_ID (cmd 17) -> contexto
3. MEM_ALLOC (cmd 5) generico -> gpu va 0x41000
4. QUEUE_GROUP_CREATE (cmd 58) -> grupo handle 0
5. MEM_ALLOC GPU_EX + REGISTER (36) + BIND (39) -> cola lista
6. QUEUE_KICK (cmd 37) -> kick aceptado
7. mmap user_io_pages + CS_ACTIVE -> monitoreo funcionando

## Pendientes

- Fase 8: instruccion STORE para confirmar ejecucion real
- Fase 9: CS_EVENT_SIGNAL para notificaciones sin polling
- Integracion con Mesa/panvk

## Descubrimientos

- GPU_EX (1<<4) obligatorio para buffers de cola CSF
- Kernel r44 separa memoria normal de ejecutable
- mmap_handle de BIND mapea 3 paginas I/O
- CS_ACTIVE en pagina output indica estado de la cola

*3 de agosto de 2026 - Fases 1 a 7 completadas*
EOF

echo ""
echo "======= TODO LISTO ======="
echo "Archivos creados:"
ls -la .gitignore README.md LICENSE docs/PROJECT_STATUS.md
