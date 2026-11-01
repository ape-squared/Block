# Handbook

Short map of Block for contributors and curious users.

## What it is

| Path | Artifact | Notes |
|------|----------|-------|
| OpenGL injector | `opengl32.dll` | Firestorm, Alchemy, Black Dragon, … (Windows) |
| Vulkan layer | `block.dll` + manifest | Win/Linux via `ENABLE_BLOCK=1`; CI on Linux |
| Metal (future) | — | macOS rebuild — see [PLATFORMS.md](PLATFORMS.md) |

**Overlay:** `Shift+B` (blocks SL UI input while open)

## Principles

1. **Honest compatibility** — only claim viewers you tested — [VIEWERS.md](VIEWERS.md)  
2. **GShade look parity** — `__GSHADE__` / `GSHADE_DITHER`; don’t invent depth overrides GShade doesn’t use  
3. **Safe addons** — no DXGI / DLSS / renodx on OpenGL installs  
4. **Human authorship** — [AI_POLICY.md](AI_POLICY.md) + PR CI gates  
5. **Credit upstream** — vkShade, ReShade FX, user GShade packs  

## Repo map

```
src/            Vulkan layer (vkShade-derived)
third_party/    Vendored ReShade (Block-branded OpenGL injector)
tools/          Build, Setup, configure
payload/        Default ini / shaders stub / addons README
brand/          Logos and marks
design/         UI + web mockups (reference)
website/        GitHub Pages + latest.json
docs/           Handbook, AI policy, VIEWERS, PLATFORMS
profiles/       App profiles
```

## Updates

`website/latest.json` is the OTA signal. Overlay and Setup fetch it and compare to `BLOCK_VERSION_*` in `third_party/reshade/include/block_version.h` (keep in sync with root `VERSION`).

Tag `vX.Y.Z` → CI refreshes `latest.json` and opens a GitHub Release.

## Also see

- [ARCHITECTURE.md](../ARCHITECTURE.md) — backends
- [NOTICE.md](../NOTICE.md) — upstream credit

## Website

Source lives in [`website/`](../website/). Deployed from `main` via `.github/workflows/pages.yml`.
