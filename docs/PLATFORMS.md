# Platforms

## Windows (primary)

| Piece | Status |
|-------|--------|
| OpenGL injector + Block Setup | **Supported** — Firestorm, Alchemy, Black Dragon, and other OpenGL SL viewers |
| Vulkan layer (`block.dll`) | Experimental — apps that load instance layers; Aurora uses a dedicated launch path |

Download: [Releases](https://github.com/ape-squared/Block/releases) → `Block-*-windows-x64.zip`.

## Linux (Vulkan layer)

The **Vulkan post-processing layer** builds on Linux (Meson / Ninja, same tree as Windows). That is the Block path for Linux today:

- Useful for Vulkan apps and testing (`ENABLE_BLOCK=1`, layer path, instance layers).  
- **Second Life viewers on Linux** are not the same “drop in `opengl32.dll`” story as Windows. OpenGL injection on Linux needs a different loader strategy and is **not** in the Setup zip yet.

CI builds the Linux Vulkan layer on pull requests so regressions show up early.

```bash
# Example — from a Linux checkout
meson setup build-linux -Dcpp_std=c++23
meson compile -C build-linux
# Stage / set VK_LAYER_PATH to the built layer (see README Vulkan section)
```

## macOS — coming soon (Metal)

Apple does not expose Vulkan as a first-class public API the way Windows and Linux do. A future **Mac build of Block** will not be a thin port of the Windows OpenGL injector.

**How Mac will work (planned):**

1. **Metal backend** — Block rebuilt against Metal (and/or a Metal-facing graphics path), not `opengl32.dll`.  
2. **New packaging** — macOS app / dylib layout, code signing, and viewer install rules specific to Apple platforms.  
3. **Same product ideas** — FX stack, presets, honest timing — on a Mac-native renderer bridge.

Until that ships, macOS is **not supported**. Star / watch the repo for announcements; do not expect the Windows zip to run under Wine as a supported configuration.

## Summary

| Platform | OpenGL SL viewers | Vulkan layer | Notes |
|----------|-------------------|--------------|--------|
| Windows | Yes (Setup) | Experimental | Download the release zip |
| Linux | Not yet | Yes (build from source / CI) | No Setup wizard yet |
| macOS | — | — | Metal rebuild planned |
