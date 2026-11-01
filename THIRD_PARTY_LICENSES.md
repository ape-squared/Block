# Third-party licenses

Block is its own product. Some open-source pieces were used under the licenses below.

---

## Vulkan path

Parts of the Vulkan layer draw on [vkShade](https://github.com/ralgar/vkShade) (BSD 2-Clause). Local pristine tree: `upstream/vkShade`. Full text: root `LICENSE`.

---

## OpenGL path

The OpenGL injector uses code from [ReShade](https://github.com/crosire/reshade) (BSD 3-Clause). Local: `third_party/reshade` — see `LICENSE.md` there.

The Vulkan FX compiler path also uses ReShade effect-compiler sources via Meson wraps.

---

## Shader packs

GShade / community shaders under `payload/reshade-shaders` — see `CREDITS.txt`. Restricted assets are not shipped by default.

---

## Other

ImGui, spdlog, glm, VMA, inih, magic_enum, stb, SPIRV-Headers, Catch2 — see `subprojects/` after configure.
