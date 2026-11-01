# Architecture — Block

Short map of how the two backends fit. For contributor ground rules see [docs/HANDBOOK.md](docs/HANDBOOK.md).

## Two products, one name

```
                    ┌─────────────────────┐
                    │   Block (product)   │
                    └──────────┬──────────┘
               ┌───────────────┴───────────────┐
               ▼                               ▼
     OpenGL injector                    Vulkan layer
     opengl32.dll                       block.dll + JSON
     (ReShade-derived)                  (vkShade-derived)
               │                               │
               ▼                               ▼
     Firestorm / OpenGL SL              Vulkan apps
     Shift+B overlay                    ENABLE_BLOCK=1
```

| Concern | OpenGL | Vulkan |
|---------|--------|--------|
| Hook | Drop-in `opengl32.dll` next to the viewer | Explicit instance layer |
| Config | `ReShade.ini` beside the DLL | Layer settings / env |
| Shaders | ReShade FX + GShade packs | FX via layer pipeline |
| Installer | Block Setup (`tools/BlockSetup.exe`) | Manual `VK_LAYER_PATH` |
| Maturity | Supported (Firestorm) | Experimental |

## OpenGL path

1. `tools/build_reshade_block.ps1` builds ReShade as Block and stages `build-win/hook/opengl32.dll`.  
2. Setup copies the injector, seeds presets/addons, writes slow-PC defaults.  
3. At runtime the overlay (`Shift+B`) owns presets, techniques, addons, and the OTA banner (`website/latest.json`).

Brand and version for updates live in `third_party/reshade/include/block_version.h` (keep in sync with root `VERSION`).

## Vulkan path

1. `tools/build_windows.bat` (Meson) produces `block.dll` and the layer manifest under `build-win/runtime/vulkan`.  
2. Apps opt in with `ENABLE_BLOCK=1` and `VK_INSTANCE_LAYERS=VK_LAYER_BLOCK_post_processing_64`.  
3. Do not point SL Vulkan viewers at this layer unless you are deliberately testing — wrong layer setup can prevent the world from rendering.

## Shared ideas

- Honest compatibility claims  
- Prefer local, inspectable presets (ini / FX) over opaque cloud sync  
- Human-authored changes only — [docs/AI_POLICY.md](docs/AI_POLICY.md)

## Repo layout

```
src/                 Vulkan layer sources
third_party/reshade  Vendored OpenGL injector
tools/               Build scripts, Setup
payload/             Default ini, shader stubs, addon notes
brand/               Logos and marks
design/              UI / web mockups (reference)
website/             GitHub Pages + latest.json
docs/                Handbook, AI policy
profiles/            App profiles
```
