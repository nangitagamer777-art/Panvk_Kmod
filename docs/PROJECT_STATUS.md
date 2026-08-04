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
