# Wire Block OpenGL injector to GShade shaders/presets/addons for a viewer folder.
# - Extracts gshade-presets\*.zip only when the pack folder is missing/empty (never wipe existing Heard).
# - Prefers the viewer's existing GShade.ini / ReShade.ini PresetPath (so active presets are not reset to Off).
# Usage:
#   .\configure_gshade_support.ps1 -ViewerDir "C:\Program Files\Firestorm-Releasex64"

[CmdletBinding()]
param(
  [string]$ViewerDir = $env:BLOCK_FIRESTORM_DIR
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
if (-not $ViewerDir) { $ViewerDir = "C:\Program Files\Firestorm-Releasex64" }
$gshade = "C:\Program Files\GShade"

if (-not (Test-Path $ViewerDir)) { throw "Viewer dir not found: $ViewerDir" }

$shaders = Join-Path $gshade "gshade-shaders"
$hasGShade = Test-Path (Join-Path $shaders "Shaders")
$presetsRoot = Join-Path $ViewerDir "gshade-presets"
$addonsRoot = Join-Path $ViewerDir "gshade-addons"
New-Item -ItemType Directory -Force -Path $presetsRoot | Out-Null
New-Item -ItemType Directory -Force -Path $addonsRoot | Out-Null

function Resolve-ZipishPath([string]$path) {
  if (-not $path) { return $null }
  $normalized = $path -replace '\\', '/'
  if ($normalized -match '\.zip(/|$)') {
    $normalized = $normalized -replace '\.zip/', '/' -replace '\.zip$', ''
    $path = $normalized -replace '/', '\'
  }
  # Relative to viewer dir
  if (-not [System.IO.Path]::IsPathRooted($path)) {
    $path = Join-Path $ViewerDir $path
  }
  if (Test-Path -LiteralPath $path) { return (Resolve-Path -LiteralPath $path).Path }
  return $null
}

function Get-IniKey([string]$file, [string]$key) {
  if (-not (Test-Path -LiteralPath $file)) { return $null }
  $m = Select-String -LiteralPath $file -Pattern ("^\s*" + [regex]::Escape($key) + "\s*=\s*(.+)$") | Select-Object -First 1
  if ($m) { return $m.Matches[0].Groups[1].Value.Trim() }
  return $null
}

function Expand-PresetZips([string]$dir) {
  Get-ChildItem $dir -Filter "*.zip" -ErrorAction SilentlyContinue | ForEach-Object {
    $packName = $_.BaseName
    $packDir = Join-Path $dir $packName
    $existing = @(Get-ChildItem $packDir -Filter "*.ini" -ErrorAction SilentlyContinue)
    if ($existing.Count -gt 0) {
      Write-Host "Keeping existing presets: $packName ($($existing.Count) ini) — skip re-extract of $($_.Name)"
      return
    }
    Write-Host "Extracting preset pack: $($_.Name) → $packName\"
    Expand-Archive -LiteralPath $_.FullName -DestinationPath $dir -Force
  }
}

Expand-PresetZips $presetsRoot

# 1) Prefer current ReShade.ini / GShade.ini PresetPath (do not reset to Off)
$preferredPreset = $null
foreach ($cfg in @((Join-Path $ViewerDir "ReShade.ini"), (Join-Path $ViewerDir "GShade.ini"))) {
  $raw = Get-IniKey $cfg "PresetPath"
  if ($raw) {
    $resolved = Resolve-ZipishPath $raw
    if ($resolved) {
      $preferredPreset = $resolved
      Write-Host "Keeping active preset from $(Split-Path $cfg -Leaf): $preferredPreset"
      break
    }
  }
}

# 2) Fallbacks — prefer a non-Off Heard preset if available
if (-not $preferredPreset) {
  $heardDir = Join-Path $presetsRoot "Heard"
  $candidates = @(
    (Join-Path $heardDir "FINAL.ini"),
    (Join-Path $heardDir "VET_Graphics_3.ini"),
    (Join-Path $heardDir "Off.ini"),
    (Join-Path $presetsRoot "Off.ini"),
    (Join-Path $presetsRoot "GShade\Off.ini")
  )
  foreach ($c in $candidates) {
    if (Test-Path -LiteralPath $c) {
      $preferredPreset = (Resolve-Path -LiteralPath $c).Path
      break
    }
  }
}

$rootOff = Join-Path $presetsRoot "Off.ini"
if (-not (Test-Path -LiteralPath $rootOff) -and $preferredPreset) {
  Copy-Item -LiteralPath $preferredPreset -Destination $rootOff -Force -ErrorAction SilentlyContinue
}

# Fresh viewers (e.g. Aurora portable) may have an empty gshade-presets — seed a minimal Off.ini
if (-not $preferredPreset) {
  New-Item -ItemType Directory -Force -Path $presetsRoot | Out-Null
  if (-not (Test-Path -LiteralPath $rootOff)) {
    @"
Techniques=
TechniqueSorting=
"@ | Set-Content -LiteralPath $rootOff -Encoding ASCII
    Write-Host "Seeded empty Off.ini under gshade-presets (no pack found)"
  }
  $preferredPreset = (Resolve-Path -LiteralPath $rootOff).Path
}

# Strip DXGI/DLSS addons that crash OpenGL viewers
Get-ChildItem $addonsRoot -File -ErrorAction SilentlyContinue |
  Where-Object { $_.Name -match 'dlss|renodx|ngx' } |
  ForEach-Object {
    Write-Host "Removing unsafe OpenGL addon: $($_.Name)"
    Remove-Item -LiteralPath $_.FullName -Force
  }

# Match GShade search order exactly. Do NOT also load viewer-local reshade-shaders when
# GShade is present — 60+ duplicate .fx names would compile twice and can bind the wrong file.
$effectPaths = New-Object System.Collections.Generic.List[string]
$texturePaths = New-Object System.Collections.Generic.List[string]
if ($hasGShade) {
  $effectPaths.Add((Join-Path $shaders "Shaders"))
  if (Test-Path "C:\Users\Public\GShade Custom Shaders\Shaders") {
    $effectPaths.Add("C:\Users\Public\GShade Custom Shaders\Shaders")
  }
  $cs = Join-Path $shaders "ComputeShaders"
  if (Test-Path $cs) { $effectPaths.Add($cs) }
  if (Test-Path "C:\Users\Public\GShade Custom Shaders\ComputeShaders") {
    $effectPaths.Add("C:\Users\Public\GShade Custom Shaders\ComputeShaders")
  }
  $texturePaths.Add((Join-Path $shaders "Textures"))
  if (Test-Path "C:\Users\Public\GShade Custom Shaders\Textures") {
    $texturePaths.Add("C:\Users\Public\GShade Custom Shaders\Textures")
  }
} else {
  foreach ($p in @(
    "C:\Users\Public\GShade Custom Shaders\Shaders",
    "C:\Users\Public\GShade Custom Shaders\ComputeShaders"
  )) { if (Test-Path $p) { $effectPaths.Add($p) } }
  if (Test-Path "C:\Users\Public\GShade Custom Shaders\Textures") {
    $texturePaths.Add("C:\Users\Public\GShade Custom Shaders\Textures")
  }
  $localShaders = Join-Path $ViewerDir "reshade-shaders\Shaders"
  $localTex = Join-Path $ViewerDir "reshade-shaders\Textures"
  if (Test-Path $localShaders) { $effectPaths.Add("$localShaders\**") } else { $effectPaths.Add(".\reshade-shaders\Shaders\**") }
  if (Test-Path $localTex) { $texturePaths.Add("$localTex\**") } else { $texturePaths.Add(".\reshade-shaders\Textures\**") }
}

$iniPath = Join-Path $ViewerDir "ReShade.ini"
$payloadIni = Join-Path $root "payload\ReShade.ini"
# Seed from GShade.ini when present so Black Dragon / GShade installs get a real ReShade.ini
if (-not (Test-Path $iniPath)) {
  $gshadeIni = Join-Path $ViewerDir "GShade.ini"
  if (Test-Path $gshadeIni) {
    Copy-Item -Force $gshadeIni $iniPath
    Write-Host "Seeded ReShade.ini from GShade.ini"
  } else {
    Copy-Item -Force $payloadIni $iniPath
  }
}

function Set-IniValue([string]$text, [string]$section, [string]$key, [string]$value) {
  $sectionPattern = "(?ms)(^\[$([regex]::Escape($section))\].*?)(?=^\[|\z)"
  $m = [regex]::Match($text, $sectionPattern)
  if (-not $m.Success) {
    if (-not $text.EndsWith("`n")) { $text += "`r`n" }
    return $text + "[$section]`r`n$key=$value`r`n"
  }
  $sec = $m.Value
  $keyPattern = "(?m)^$([regex]::Escape($key))=.*$"
  if ([regex]::IsMatch($sec, $keyPattern)) {
    $newSec = [regex]::Replace($sec, $keyPattern, "$key=$value", 1)
  } else {
    $newSec = $sec.TrimEnd("`r", "`n") + "`r`n$key=$value`r`n"
  }
  return $text.Remove($m.Index, $m.Length).Insert($m.Index, $newSec)
}

$ini = Get-Content $iniPath -Raw
if (-not $ini) { $ini = Get-Content $payloadIni -Raw }

if (-not $preferredPreset) { throw "No preset INI found under $presetsRoot" }

# Prefer GShade's Intermediate folder so we reuse the same compiled shaders as GShade (look + speed).
$gshadeIntermediate = Join-Path $shaders "Intermediate"
if ($hasGShade -and (Test-Path $gshadeIntermediate)) {
  $cacheRoot = $gshadeIntermediate
} else {
  $cacheRoot = Join-Path $env:LOCALAPPDATA "Block\cache"
  New-Item -ItemType Directory -Force -Path $cacheRoot | Out-Null
}

# Optional DisabledAddons from env (comma-separated), else keep existing
$disabledAddons = Get-IniKey $iniPath "DisabledAddons"
if (-not $disabledAddons) { $disabledAddons = "" }

# Visual parity with GShade:
# - Same EffectSearchPaths order (no duplicate viewer-local shaders).
# - Same Intermediate cache, UseDithering, EffectFrameDelay, DEPTH heuristics.
# - Never force PerformanceMode (kills uniform parity with GShade).
# - Block injects __GSHADE__ + GSHADE_DITHER in the FX compiler.
$ini = Set-IniValue $ini "GENERAL" "EffectSearchPaths" ($effectPaths -join ",")
$ini = Set-IniValue $ini "GENERAL" "TextureSearchPaths" ($texturePaths -join ",")
$ini = Set-IniValue $ini "GENERAL" "PresetPath" $preferredPreset
$ini = Set-IniValue $ini "GENERAL" "IntermediateCachePath" $cacheRoot
$ini = Set-IniValue $ini "GENERAL" "PreprocessorDefinitions" ""
$ini = Set-IniValue $ini "GENERAL" "UseDithering" "1"
$ini = Set-IniValue $ini "GENERAL" "NoReloadOnInit" "0"
$ini = Set-IniValue $ini "GENERAL" "NoDebugInfo" "1"
$ini = Set-IniValue $ini "GENERAL" "NoEffectCache" "0"
$ini = Set-IniValue $ini "GENERAL" "SkipLoadingDisabledEffects" "0"
$ini = Set-IniValue $ini "GENERAL" "PerformanceMode" "0"
$ini = Set-IniValue $ini "GENERAL" "EffectFrameDelay" "500"
$ini = Set-IniValue $ini "GENERAL" "PresetTransitionDuration" "0"

# Copy Generic Depth heuristics from GShade.ini (MXAO / DOF look wrong without these).
function Get-IniSectionKey([string]$file, [string]$section, [string]$key) {
  if (-not (Test-Path -LiteralPath $file)) { return $null }
  $raw = Get-Content -LiteralPath $file -Raw
  $sectionPattern = "(?ms)^\[$([regex]::Escape($section))\].*?(?=^\[|\z)"
  $sm = [regex]::Match($raw, $sectionPattern)
  if (-not $sm.Success) { return $null }
  $km = [regex]::Match($sm.Value, "(?m)^\s*$([regex]::Escape($key))\s*=\s*(.+)$")
  if ($km.Success) { return $km.Groups[1].Value.Trim() }
  return $null
}
$gshadeIniFile = Join-Path $ViewerDir "GShade.ini"
if (Test-Path $gshadeIniFile) {
  foreach ($sec in @("DEPTH", "DEPTH_II")) {
    foreach ($key in @("DrawStatsHeuristic", "FilterFormat", "UseAspectRatioHeuristics", "UseDefaultDepth", "DepthCopyBeforeClears")) {
      $v = Get-IniSectionKey $gshadeIniFile $sec $key
      if ($null -ne $v -and $v -ne "") {
        $ini = Set-IniValue $ini $sec $key $v
      }
    }
  }
} else {
  # Sensible SL defaults matching typical GShade Firestorm installs
  $ini = Set-IniValue $ini "DEPTH" "DrawStatsHeuristic" "2"
  $ini = Set-IniValue $ini "DEPTH" "FilterFormat" "5"
  $ini = Set-IniValue $ini "DEPTH" "UseAspectRatioHeuristics" "3"
  $ini = Set-IniValue $ini "DEPTH" "UseDefaultDepth" "0"
}
$ini = Set-IniValue $ini "ADDON" "AddonPath" $addonsRoot
$ini = Set-IniValue $ini "ADDON" "DisabledAddons" $disabledAddons
# 2 = block all input while overlay is visible (clicks must not hit the SL UI underneath)
$ini = Set-IniValue $ini "INPUT" "InputProcessing" "2"
$ini = Set-IniValue $ini "INPUT" "ForceShortcutModifiers" "1"
$ini = Set-IniValue $ini "INPUT" "KeyOverlay" "66,0,1,0"
$ini = Set-IniValue $ini "STYLE" "StyleIndex" "2"
$ini = Set-IniValue $ini "STYLE" "FontSize" "18.000000"
$ini = Set-IniValue $ini "STYLE" "EditorFontSize" "18.000000"
$ini = Set-IniValue $ini "STYLE" "FontScale" "1.000000"
$ini = Set-IniValue $ini "OVERLAY" "VariableListHeight" "280.000000"
$ini = Set-IniValue $ini "OVERLAY" "VariableListUseSliders" "1"
$ini = Set-IniValue $ini "OVERLAY" "TutorialProgress" "4"

Set-Content -Path $iniPath -Value $ini -NoNewline

# Only clear Block's private cache — never wipe GShade's Intermediate (tens of thousands of files).
$blockCache = Join-Path $env:LOCALAPPDATA "Block\cache"
if ((Test-Path $blockCache) -and ($cacheRoot -eq $blockCache)) {
  Get-ChildItem $blockCache -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
  Write-Host "Cleared Block effect cache: $blockCache"
}

$heardCount = @(Get-ChildItem (Join-Path $presetsRoot "Heard") -Filter "*.ini" -ErrorAction SilentlyContinue).Count
$presetCount = @(Get-ChildItem $presetsRoot -Recurse -Filter "*.ini" -ErrorAction SilentlyContinue).Count
$addonCount = @(Get-ChildItem $addonsRoot -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -match '^\.addon' }).Count

Write-Host "Configured: $ViewerDir"
Write-Host "  PresetPath -> $preferredPreset"
Write-Host "  Heard presets: $heardCount"
Write-Host "  Presets total: $presetCount under $presetsRoot"
Write-Host "  AddonPath  -> $addonsRoot ($addonCount addon file(s))"
Write-Host "  Look       -> GShade parity (PerformanceMode=0, UseDithering=1, DEPTH from GShade.ini, FontSize=18)"
Write-Host "  Cache      -> $cacheRoot"
Write-Host "  Paths      -> $($effectPaths -join ' | ')"
if ($hasGShade) { Write-Host "  Shaders    -> $shaders (+ custom; viewer-local reshade-shaders skipped)" }

# Keep staged OpenGL pack defaults from payload (do not overwrite with viewer-absolute paths)
$runtimeIni = Join-Path $root "build-win\runtime\opengl\ReShade.ini"
if ((Test-Path (Split-Path $runtimeIni)) -and (Test-Path $payloadIni)) {
  Copy-Item -Force $payloadIni $runtimeIni
}
