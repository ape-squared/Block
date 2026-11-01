# ReShade Presets

Block supports ReShade-like preset files (`ReShade.ini`).

The preset file is searched for in the same locations as `Block.ini`, or a
custom file can be specified with the `BLOCK_PRESET_FILE` / `VKSHADE_PRESET_FILE`
environment variable.

Uniform values are loaded from the preset and applied to effects at runtime.

!!! TIP
    Config and preset files hot-reload. Effect sources (`.fx` / includes) also
    hot-reload when changed on disk.

## Techniques

```ini
Effects=SMAA.fx,MXAO.fx
Techniques=SMAA@SMAA.fx,MXAO@MXAO.fx
TechniqueSorting=SMAA@SMAA.fx,MXAO@MXAO.fx
```

`Techniques` / `TechniqueSorting` use ReShade's `Name@File.fx` form. If only
`Effects=` is set, every technique in those files is enabled (legacy behaviour).

## Preprocessor definitions

```ini
PreprocessorDefinitions=RESHADE_DEPTH_INPUT_IS_REVERSED=1
[SMAA.fx]
PreprocessorDefinitions=SMAA_PRESET=1
```
