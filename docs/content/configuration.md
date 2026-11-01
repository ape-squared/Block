# Configuration

The `Block.ini` configuration file is searched for in the following
 locations, in order of precedence:

- A file set with the environment variable
  `BLOCK_CONFIG_FILE=/path/to/Block.ini` (also accepts legacy `VKSHADE_CONFIG_FILE`)
- A file named `Block.ini` (or legacy `vkShade.ini`) in the working directory of the game
- Platform config/data directories under `Block` / `vkShade`

!!! TIP
    The default/example configuration file is `config/Block.ini`.

## ReShade FX

```ini title="Block.ini"
[ReShade]
EffectSearchPaths = shaders,effects
TextureSearchPaths = textures
PreprocessorDefinitions = RESHADE_DEPTH_INPUT_IS_REVERSED=1
```

## Input

```ini title="Block.ini"
[Input]
ToggleEffects = KEY_INSERT
ToggleGui = Shift+KEY_B
# 0 = passthrough, 1 = block when ImGui wants capture, 2 = block while overlay open
InputProcessing = 1
```

Bindings accept `Ctrl+`, `Alt+`, and `Shift+` modifiers.

## HDR

```ini title="Block.ini"
[HDR]
ForceHDR = 0
Mode = scrgb
PaperWhiteNits = 203
```

When `ForceHDR=1`, the layer requests an HDR swapchain format/color-space pair
(`scrgb` → `R16G16B16A16_SFLOAT` + `EXTENDED_SRGB_LINEAR_EXT`, or `hdr10`).
If the surface rejects the pair, the app format is kept.

## Depth

```ini title="Block.ini"
[Depth]
UseAspectRatioHeuristics = 1
DrawStatsHeuristic = 1
```

## Log Output

Use `BLOCK_LOG_LEVEL` / legacy `VKSHADE_LOG_LEVEL` (`trace`…`off`) and
`BLOCK_LOG_FILE` / `VKSHADE_LOG_FILE` for file output.
