# Changelog

All notable changes to Block are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)-style sections under each version.

## [0.3.3] — 2026-11-01

### Added

- **Vulkan depth tracking** — heuristic depth-buffer discovery and copy so depth-aware FX can sample scene depth (status in overlay; full DEPTH_II picker still later).
- **Vulkan multi-technique effects** — `Techniques` / `TechniqueSorting` presets run every enabled technique in order, not just the first pass.
- **Vulkan runtime uniforms** — ReShade-style input and overlay uniforms (`MousePoint`, `FrameTime`, overlay rects, etc.) wired through the Win32 input path.
- **Input capture (`InputProcessing`)** — when the overlay is open, viewer input can be captured so clicks stay in Block instead of the world.
- **Effect hot-reload** — FX source and include changes reload without restarting the host (Vulkan runtime).
- **Forced HDR presentation option** — swapchain format rewrite path for reverse-tonemap / HDR-targeted shaders (not a full SDR proxy pipeline).
- **Overlay favorites & recent presets** — quicker jumps to presets you actually use.
- **Depth-missing call-to-action** in the overlay when depth FX need a buffer that isn’t available yet.
- **Setup post-install CTA** — clearer next step after inject (launch viewer → Shift+B).
- **Website UI smoke tests** (Playwright) on every PR.

### Changed

- **GShade preset framing** — packs under `gshade-presets\` (including folders like `Heard`) are treated as normal GShade preset sets, not a separate “Heard presets” product.
- **Overlay branding** — Block lockup, fonts, and StyleIndex “Block” theme instead of generic ReShade chrome.
- **Setup copy & version sync** — product version follows `VERSION` / `block_version.h`; install messaging matches the GShade-pack wording.
- **Docs & site** — README comparison vs GShade / ReShade / Vulkan layers; handbook status, presets, and configuration pages updated for the current feature set.
- **OTA `latest.json` notes** — release channel text now summarizes Added / Changed / Improved instead of a one-line stub.

### Improved

- **Vulkan FX image path** — stronger image create/barrier handling for effect I/O and present sync.
- **Win32 input backend** — more reliable mouse deltas, button codes, and virtual-key mapping for overlay + uniform consumers.
- **Effects panel UX** — clearer technique/preset controls for the OpenGL injector overlay.
- **Release packaging** — Windows zip still ships Setup + OpenGL injector; changelog and GitHub Release body stay in sync.

### Notes

- **Windows OpenGL** (Firestorm, Alchemy, Black Dragon, …) remains the supported path via Setup.
- **Vulkan** layer remains experimental; build from source on Linux (Meson).
- **macOS** not supported yet (future Metal rebuild).

## [0.3.2] — 2026-11-01

### Added

- Depth II-oriented depth persist/check hooks in the OpenGL overlay path.
- Favorites / recent preset shortcuts; pin effects; pause processing when the viewer is unfocused.

### Changed

- Safer addons catalog handling; Trim tip copy in Setup/overlay.

### Improved

- Depth-related UX clarity for OpenGL viewers.

## [0.3.0] — 2026-10-31

### Added

- GShade look parity (effect/preset paths + DEPTH-oriented defaults).

### Changed

- Block lime loader branding; less ReShade chrome in the overlay.

## [0.2.x] — 2026-10-31

Early OpenGL Setup + injector releases for Second Life viewers (Firestorm and friends). See GitHub Releases for zip history.
