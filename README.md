# PanVK-kbase — Vulkan Driver for Mali Valhall GPUs

Pre-built Vulkan ICD driver for ARM Mali Valhall GPUs with CSF firmware,
using the proprietary kbase kernel driver via `/dev/mali0`.

No custom kernel, no blobs.

## Supported GPUs

| Generation | Models |
|-----------|--------|
| Gen 3 | Mali-G510, Mali-G610, Mali-G615, Mali-G710 |
| Gen 4 | Mali-G715, Mali-G720, Mali-G725, Mali-G620, Mali-G625 |
| Gen 5+ | Mali-G730, Mali-G820 (CSF-based, likely compatible) |

Primary test device: **Mali-G615 MC6** on MediaTek Dimensity 8300 (Poco X6 Pro).

## Status

`vkCreateDevice` returns `VK_SUCCESS` on Mali-G615 MC6.
CSF queue lifecycle working: REGISTER -> BIND -> KICK with 3 subqueues.

## Install

Load the ZIP directly in your Vulkan loader as an ICD package.

## Source

Mesa patches and backend source: [PanVK-kbase](https://github.com/nangitagamer777-art/PanVK-kbase)

## Credits

**Noin Haxel** ([@nangitagamer777-art](https://github.com/nangitagamer777-art))

## License

MIT
