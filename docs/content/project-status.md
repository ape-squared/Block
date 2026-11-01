# Project Status

--8<--
README.md:project-status
--8<--

## Current Features

See also [Limitations](#limitations).

- [x] ReShade FX shader support
- [x] Configuration system with ReShade-like presets and hot-reloading
- [x] Interactive GUI overlay with keyboard and mouse support
- [x] Cross-platform input (Wayland, Xlib, XCB)
- [x] Input capture (`InputProcessing`) and ReShade input/overlay runtime uniforms
- [x] Multi-technique effects (`Techniques` / `TechniqueSorting`)
- [x] Hot-reloadable effects (source + includes)
- [x] Forced HDR presentation option
- [x] Depth buffer access (heuristic copy)

## Limitations

Some ReShade features are still incomplete:

- Compute passes / storage images
- Full DEPTH_II-style depth picker UI (status indicator only)
- MSAA depth resolve
- Linux input capture (Win32 subclass capture only)

## Roadmap

### Near-term (v0.2.x+)

- [x] Improve input handling and add capture support
- [x] Improve ReShade FX image compatbility
- [x] Support forced HDR presentation (for reverse tonemapping shaders)
- [x] Support effects with multiple techniques
- [x] Support ReShade's input runtime uniforms
- [x] Support ReShade's overlay runtime uniforms
- [x] Hot-reloadable effects
- [x] Depth buffer access

### Long-term

- [ ] Community effect repository
- [ ] Performance profiling tools
