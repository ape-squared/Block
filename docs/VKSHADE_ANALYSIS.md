# vkShade analysis + dual-backend notes

Upstream mirror: `upstream/vkShade` @ `d926d78` (v0.1.0)

## Status

| Item | Result |
|------|--------|
| Vulkan `block.dll` build | PASS |
| Vulkan runtime (vkcube) | PASS |
| OpenGL `opengl32.dll` / `Block64.dll` build | PASS |
| Firestorm OpenGL injector load | PASS (loaded into Firestorm-Releasex64.exe; see BASELINE) |

## Dual-backend

- **Vulkan:** vkShade-derived Khronos layer (`ENABLE_BLOCK`, Win32 surface/input).
- **OpenGL:** ReShade injector restored from `archive/reshade-era` → `third_party/reshade`.

Firestorm world rendering is OpenGL. The Vulkan layer is **not** the Firestorm path.

## What vkShade already did

Vulkan layer, ReShadeFX→SPIR-V, INI/presets, ImGui overlay, Linux input.

## What Block added

Windows Vulkan port + branding; ApplicationProfile stubs; OpenGL ReShade backend for Firestorm.
