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

## Limitations

Some ReShade effects and preset features do not work properly yet. For example:

- Effects relying on the depth buffer
- Effects with multiple techniques
- ReShade's `Techniques` and `TechniqueSorting` preset parameters
- Possibly more...

## Roadmap

### Near-term (v0.2.x+)

- [ ] Improve input handling and add capture support
- [ ] Improve ReShade FX image compatbility
- [ ] Support forced HDR presentation (for reverse tonemapping shaders)
- [ ] Support effects with multiple techniques
- [ ] Support ReShade's input runtime uniforms
- [ ] Support ReShade's overlay runtime uniforms
- [ ] Hot-reloadable effects
- [ ] Depth buffer access

### Long-term

- [ ] Community effect repository
- [ ] Performance profiling tools
