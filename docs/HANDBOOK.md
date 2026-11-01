# Block handbook

Short accompanying document for contributors and curious users.

## What Block is

| Path | Artifact | Use |
|------|----------|-----|
| OpenGL injector | `opengl32.dll` | Firestorm and other OpenGL Second Life viewers |
| Vulkan layer | `block.dll` + `Block_x86_64.json` | Vulkan apps (`ENABLE_BLOCK=1`); **not** Aurora-ready |

Overlay (OpenGL): **Shift+B**.

## Product principles

1. **Honest compatibility** — never market a viewer we have not tested.
2. **Slow-PC defaults** — `PerformanceMode=1`, `SkipLoadingDisabledEffects=1`, compile cache under `%LOCALAPPDATA%\Block\cache`.
3. **Safe addons** — OpenGL installs must not load DXGI/DLSS/renodx addons.
4. **Human authorship** — see [AI_POLICY.md](AI_POLICY.md).
5. **Credit upstream** — vkShade (Ryan Algar), ReShade FX ecosystem, GShade presets when the user provides them.

## Repo map

```
src/           Vulkan layer (vkShade-derived)
third_party/   Vendored ReShade (Block-branded OpenGL injector source)
tools/         Build, Setup, configure scripts
payload/       Default ReShade.ini, shaders stub, addons README
brand/         Logos and marks
design/        UI / web mockups (reference)
website/       GitHub Pages one-pager
docs/          Handbook, AI policy, baseline notes
profiles/      App profiles
```

## Aurora (read this)

Aurora Viewer is Vulkan-native. Forcing `VK_INSTANCE_LAYERS=VK_LAYER_BLOCK_post_processing_64` makes Aurora lose the NVIDIA adapter and fail surface creation. Launch Aurora **without** Block until that is fixed. Firestorm remains the supported SL path.

## Website

Source: [`website/`](../website/). Published via GitHub Pages from the `website` folder on `main`.
