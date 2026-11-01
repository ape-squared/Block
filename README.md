# Block

Post-processing for **Second Life viewers** (OpenGL) and **Vulkan** apps on Windows.

| Backend | Artifact | Status |
|---------|----------|--------|
| OpenGL | `opengl32.dll` via Block Setup | **Supported** — Firestorm |
| Vulkan | `block.dll` layer | Experimental — works in vkcube; **not** Aurora-safe yet |

**Overlay:** Shift+B · **Licence:** BSD-2-Clause · **Site:** [ape-squared.github.io/block](https://ape-squared.github.io/block/)

> **Human craft.** We do not accept AI-generated contributions. See [docs/AI_POLICY.md](docs/AI_POLICY.md) and [CONTRIBUTING.md](CONTRIBUTING.md).

## Quick start — OpenGL (Firestorm)

```bat
powershell -ExecutionPolicy Bypass -File tools\build_reshade_block.ps1
powershell -ExecutionPolicy Bypass -File tools\stage_block_runtime.ps1
powershell -ExecutionPolicy Bypass -File tools\build_block_setup.ps1
tools\BlockSetup.exe
```

Setup installs the injector, wires GShade / Heard presets, enables slow-PC defaults (`PerformanceMode`, compile-only-active effects, `%LOCALAPPDATA%\Block\cache`), and offers an OpenGL-safe addon picker (DLSS/renodx blocked).

## Quick start — Vulkan (experimental)

```bat
tools\build_windows.bat
tools\stage_block_runtime.ps1
```

```bat
set ENABLE_BLOCK=1
set VK_LAYER_PATH=%CD%\build-win\runtime\vulkan
set VK_INSTANCE_LAYERS=VK_LAYER_BLOCK_post_processing_64
vkcube
```

Do **not** force this layer on Aurora Viewer — it breaks GPU init. Details: [docs/HANDBOOK.md](docs/HANDBOOK.md).

## Docs

| Doc | What |
|-----|------|
| [website/](website/) | Product one-pager (GitHub Pages) |
| [docs/HANDBOOK.md](docs/HANDBOOK.md) | Repo map & principles |
| [docs/AI_POLICY.md](docs/AI_POLICY.md) | Stance against AI-authored contributions |
| [CONTRIBUTING.md](CONTRIBUTING.md) | How to contribute |
| [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) | Notices |

## Credits

Incorporates and evolves [vkShade](https://github.com/ralgar/vkShade) (Ryan Algar, BSD-2-Clause). OpenGL path builds on [ReShade](https://reshade.me/). Not affiliated with Linden Lab, Second Life, ReShade, or GShade.
