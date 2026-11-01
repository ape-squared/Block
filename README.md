<p align="center">
  <img src="brand/01-logos/lockup/block-lockup-on-dark-240.png" alt="Block" width="280">
</p>

# Block

**Build your look, block by block.**  
Post-processing for Second Life viewers (OpenGL) on Windows, Vulkan on Windows/Linux — open source, human-made. macOS Metal coming soon.

[![Website](https://img.shields.io/badge/website-ape--squared.github.io%2FBlock-D4FF3F?labelColor=0B0C0E&color=D4FF3F)](https://ape-squared.github.io/Block/)
[![Release](https://img.shields.io/badge/release-v0.3.0-5BE3E8?labelColor=0B0C0E)](https://github.com/ape-squared/Block/releases/tag/v0.3.0)
[![License: BSD-2-Clause](https://img.shields.io/badge/license-BSD--2--Clause-4F46E5?labelColor=0B0C0E)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20·%20Linux-070B1F?labelColor=0B0C0E)](#using-block)
[![CI](https://img.shields.io/badge/CI-PR%20checks-070B1F?labelColor=0B0C0E)](https://github.com/ape-squared/Block/actions/workflows/pr-checks.yml)

> This software is not provided, endorsed, or supported by Linden Lab, the makers of Second Life.  
> Not affiliated with ReShade or GShade.

[Why Block](#why-block) · [What works today](#what-works-today) · [Using Block](#using-block) · [Build](#build) · [Updates](#updates) · [Contributing](#contributing) · [Docs](#docs)

---

**Stack colour grading, bloom, sharpening and more — and see the cost in milliseconds.**  
**Firestorm, Alchemy, Black Dragon, and other OpenGL SL viewers on Windows today.**

> **Status:** early (0.x). OpenGL Setup path is supported on Firestorm, Alchemy, and Black Dragon. Linux builds the Vulkan layer (Meson). macOS = future Metal rebuild. See [docs/VIEWERS.md](docs/VIEWERS.md) and [docs/PLATFORMS.md](docs/PLATFORMS.md).

> **Human craft only.** We do not accept AI-generated contributions. PRs run authorship checks. Read [docs/AI_POLICY.md](docs/AI_POLICY.md).

## Why Block

| | |
|---|---|
| **OpenGL first** | Injector for Firestorm, Alchemy, Black Dragon, and friends |
| **GShade look parity** | `__GSHADE__` / `GSHADE_DITHER` macros so Heard presets match GShade |
| **Honest cost** | Per-effect timings so you know what you are paying for |
| **Your shader packs** | GShade / ReShade FX presets; Heard and friends via Setup |
| **Safe addons** | OpenGL installs skip DXGI / DLSS / renodx; pick what you load |
| **One key overlay** | `Shift`+`B` — input blocked from the SL UI while open |

## What works today

| Backend | Artifact | Status |
|---------|----------|--------|
| **OpenGL (Windows)** | `opengl32.dll` via Block Setup | Supported — Firestorm, Alchemy, Black Dragon |
| **Vulkan (Win / Linux)** | `block.dll` / Meson layer | Experimental / CI builds on Linux |
| **Metal (macOS)** | Future rebuild | Coming soon — not this zip |

**Overlay:** `Shift`+`B` · **Version:** [`VERSION`](VERSION) · **Licence:** [BSD-2-Clause](LICENSE)

### OpenGL (Windows viewers)

- Block Setup finds Firestorm, Alchemy, Black Dragon, Cool VL, Singularity, Kokua, …  
- Wires GShade shaders / Heard presets when present  
- Overlay blocks clicks from falling through into Second Life  
- Addon picker that stays OpenGL-safe  
- Update check against GitHub Pages `latest.json`

### Vulkan (experimental) · Linux

- `VK_LAYER_BLOCK_post_processing_64` for apps that load instance layers  
- Linux: build with Meson (see [docs/PLATFORMS.md](docs/PLATFORMS.md)); CI compiles on every PR  
- Derived from [vkShade](https://github.com/ralgar/vkShade) — see [NOTICE.md](NOTICE.md)

### macOS — coming soon

A future Mac Block is a **Metal** rebuild (native bridge + packaging), not Wine of the Windows zip.

## Using Block

1. Get a release or build from source ([Build](#build)).  
2. Run **Block Setup** and check every OpenGL viewer you use.  
3. Launch the viewer. In-world, press **`Shift`+`B`**.  
4. Browse presets (e.g. `gshade-presets\Heard\`) and toggle techniques.

Logs: `ReShade.log` next to the viewer (OpenGL). Prefer pasting log text in issues — not screenshots of screenshots.

## Build

### Windows (OpenGL injector — primary)

Visual Studio Build Tools (MSVC) required.

```bat
powershell -ExecutionPolicy Bypass -File tools\build_reshade_block.ps1
powershell -ExecutionPolicy Bypass -File tools\stage_block_runtime.ps1
powershell -ExecutionPolicy Bypass -File tools\build_block_setup.ps1
tools\BlockSetup.exe
```

Vulkan layer on Windows (experimental):

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

### Linux (Vulkan layer)

```bash
meson setup build-linux -Dcpp_std=c++23
meson compile -C build-linux
```

Details: [docs/PLATFORMS.md](docs/PLATFORMS.md).

## Updates

When a new version ships:

1. Bump [`VERSION`](VERSION) and `third_party/reshade/include/block_version.h`  
2. Tag `vX.Y.Z` — CI writes [`website/latest.json`](website/latest.json) and opens a GitHub Release  
3. Setup and the overlay (`Shift`+`B`) show **There's a new Block update available** with a link to Releases  

Channel: https://ape-squared.github.io/Block/latest.json

## Contributing

Open contributions welcome — from **humans**.

- Read [CONTRIBUTING.md](CONTRIBUTING.md) and [docs/AI_POLICY.md](docs/AI_POLICY.md)  
- Be honest about viewers; do not claim untested support  
- One concern per PR; say what you tested (viewer + GPU)  

Pull requests use the [PR template](.github/PULL_REQUEST_TEMPLATE.md). Bugs use the [issue form](https://github.com/ape-squared/Block/issues/new/choose).

## Docs

| | |
|--|--|
| [website/](website/) | Product site (GitHub Pages) |
| [docs/VIEWERS.md](docs/VIEWERS.md) | Firestorm, Alchemy, Black Dragon, … |
| [docs/PLATFORMS.md](docs/PLATFORMS.md) | Windows · Linux · macOS / Metal |
| [docs/HANDBOOK.md](docs/HANDBOOK.md) | Repo map and principles |
| [docs/AI_POLICY.md](docs/AI_POLICY.md) | Stance against AI-authored contributions |
| [ARCHITECTURE.md](ARCHITECTURE.md) | How the two backends fit together |
| [NOTICE.md](NOTICE.md) | Upstream credit and provenance |
| [CONTRIBUTING.md](CONTRIBUTING.md) | How to contribute |
| [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) | Licence notices |

## Credits

Incorporates and evolves [vkShade](https://github.com/ralgar/vkShade) (Ryan Algar, BSD-2-Clause).  
OpenGL path builds on [ReShade](https://reshade.me/).  
Not affiliated with Linden Lab, Second Life, ReShade, or GShade.
