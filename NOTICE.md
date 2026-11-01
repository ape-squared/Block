# Notices — Block

Block is free software under the **BSD 2-Clause License** (see [LICENSE](LICENSE)).

Block is not provided, endorsed, or supported by Linden Research, Inc. ("Linden Lab"), The Phoenix Firestorm Project, Inc., crosire (ReShade), or the GShade project. Second Life® is a trademark of Linden Research, Inc.

## Upstream code

| Piece | Origin | Licence | Where in this tree |
|-------|--------|---------|-------------------|
| Vulkan post-processing layer | [vkShade](https://github.com/ralgar/vkShade) — Ryan Algar | BSD-2-Clause | `src/`, pristine reference in `upstream/vkShade` |
| OpenGL injector / FX runtime | [ReShade](https://github.com/crosire/reshade) | BSD-3-Clause | `third_party/reshade` (Block-branded) |
| Effect compiler (FX) | ReShade / ReShadeFX | BSD-3-Clause | Meson wraps + ReShade tree |

Full licence texts and additional dependency notes: [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

## Shader packs and presets

GShade / community shaders and presets are **not** Block's authorship. Setup can wire folders the user already has (`gshade-shaders`, `gshade-presets`). Restricted assets are not shipped by default — see `payload/reshade-shaders` and any `CREDITS.txt` next to packs you install.

## Brand

Block wordmark and lime signal colour are part of this project. Do not imply Linden Lab, ReShade, or GShade endorsement when you reuse them.
