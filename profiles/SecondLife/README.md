# SecondLife / Firestorm profile

Primary demo target: **Firestorm** on Windows.

## Rendering reality

| Path | Applicable? |
|------|-------------|
| Vulkan layer (`block.dll`) | **No** for world rendering — Firestorm uses **OpenGL** |
| OpenGL injector (`opengl32.dll` / `Block64.dll`) | **Yes** — this is the Firestorm path |

`SecondLifeProfile::detect()` in the Vulkan layer always returns **false**. Do not treat Vulkan layer load as SL support.

## Install (OpenGL)

1. `tools\build_reshade_block.ps1`
2. `tools\stage_block_runtime.ps1`
3. Copy from `build-win\runtime\opengl\` next to `Firestorm-Releasex64.exe`:
   - `opengl32.dll`
   - `ReShade.ini`
   - `reshade-shaders\`
4. Launch Firestorm. Overlay: **Shift+B**.

Install root on this machine:

```text
C:\Program Files\Firestorm-Releasex64\
```

## Status

Injector load smoke test: **PASS** (see `docs/BASELINE.md` and `logs/block-firestorm-opengl.log`).
Full in-world effect QA is separate and not claimed by the smoke test alone.
