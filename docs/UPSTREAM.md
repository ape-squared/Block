# Upstream notes

Block is its own product. A bit of open-source work was reused for each backend.

## Vulkan

Some layer structure and FX plumbing came from [vkShade](https://github.com/ralgar/vkShade) (`d926d78`, v0.1.0). Pristine mirror: `upstream/vkShade`.

Internal symbols may still say `vkShade_*` for easier sync. Product surface is Block (`block.dll`, `ENABLE_BLOCK`, overlay title).

## OpenGL

Firestorm needs an OpenGL injector. That path uses ReShade’s injector sources under `third_party/reshade`, branded as Block (`opengl32.dll`).

`tools/brand_reshade_as_block.ps1` keeps the UI product-first; licenses stay in `THIRD_PARTY_LICENSES.md` / About → License.

## Notable Block changes (Vulkan)

| Change | Why |
|--------|-----|
| Meson project Block; `block.dll` | Product identity |
| MSVC export + Win32 surface/input | Windows Vulkan |
| `ApplicationProfile` stubs | Honest SL detection |
| `BLOCK_*` env with `VKSHADE_*` fallbacks | Branding + migration |
