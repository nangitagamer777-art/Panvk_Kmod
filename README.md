# PanVK-kbase — Vulkan Driver for ARM Mali Valhall GPUs

Pre-built Vulkan ICD driver for ARM Mali Valhall GPUs with CSF firmware,
using the proprietary `mali_kbase` kernel driver via `/dev/mali0`.

No custom kernel, no blobs, no root required (Shizuku compatible).

## Supported GPUs

| Generation | Models |
|-----------|--------|
| Gen 3 | Mali-G510, Mali-G610, Mali-G615, Mali-G710 |
| Gen 4+ | Mali-G715, Mali-G720, Mali-G725, Mali-G620, Mali-G625 |

Tested on: **Mali-G615 MC6** / MediaTek Dimensity 8300 (Poco X6 Pro).

## Status

vkCreateDevice returns VK_SUCCESS (August 2026).
CSF queue lifecycle working: REGISTER -> BIND -> KICK.
Not yet ready for rendering.

## Installation

1. Download `PanVK_kbase_Mali_Valhall.zip`
2. Extract to a directory accessible by your Vulkan loader
3. Configure Vulkan ICD path to point to the extracted files
4. LD_PRELOAD or system push `libkbase_drm.so` alongside the driver

## Files in package

- `libvulkan_panfrost.so` — Mesa PanVK driver with kbase backend
- `libkbase_drm.so` — DRM compatibility shim (LD_PRELOAD)
- `meta.json` — Vulkan ICD manifest

## Requirements

- ARM64 Android device with `/dev/mali0`
- Shizuku, ADB shell, or root to access the device node
- Vulkan 1.4 loader

## Source Code

The Mesa patches and kbase backend source are maintained in:
[PanVK-kbase](https://github.com/nangitagamer777-art/PanVK-kbase)

## Credits

Noin Haxel (@nangitagamer777-art)

## License

MIT
