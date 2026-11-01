# Build a portable Block zip a new PC can download: Setup + OpenGL injector + configure script.
# Layout matches what BlockSetup.exe expects (FindRepoRoot → tools\configure_gshade_support.ps1).
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$ver = (Get-Content (Join-Path $root "VERSION") -Raw).Trim()
if (-not $ver) { throw "VERSION missing" }

Write-Host "Building Setup..."
& (Join-Path $root "tools\build_block_setup.ps1")
if ($LASTEXITCODE -ne 0) { throw "build_block_setup failed" }

$glDll = Join-Path $root "build-win\hook\opengl32.dll"
if (-not (Test-Path $glDll)) {
  Write-Host "OpenGL DLL missing — building injector..."
  & (Join-Path $root "tools\build_reshade_block.ps1")
  if ($LASTEXITCODE -ne 0) { throw "build_reshade_block failed" }
}

$glOut = Join-Path $root "build-win\runtime\opengl"
New-Item -ItemType Directory -Force -Path $glOut | Out-Null
Copy-Item -Force (Join-Path $root "build-win\hook\opengl32.dll") $glOut
foreach ($n in @("Block64.dll", "dxgi.dll", "d3d11.dll", "d3d12.dll", "ReShade.ini")) {
  $p = Join-Path $root "build-win\hook\$n"
  if (Test-Path $p) { Copy-Item -Force $p $glOut }
}

$stage = Join-Path $root "build-win\dist\Block-$ver-windows-x64"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

# Entry point at zip root
Copy-Item -Force (Join-Path $root "tools\BlockSetup.exe") (Join-Path $stage "BlockSetup.exe")
$stageAssets = Join-Path $stage "Assets"
New-Item -ItemType Directory -Force -Path $stageAssets | Out-Null
Copy-Item -Force (Join-Path $root "tools\Assets\*") $stageAssets -ErrorAction SilentlyContinue
if (-not (Test-Path (Join-Path $stageAssets "lockup.png"))) {
  Copy-Item -Force (Join-Path $root "tools\setup\Assets\*") $stageAssets
}

# Repo-shaped tree so FindRepoRoot + InjectorDll resolve
New-Item -ItemType Directory -Force -Path (Join-Path $stage "tools") | Out-Null
Copy-Item -Force (Join-Path $root "tools\configure_gshade_support.ps1") (Join-Path $stage "tools\")
Copy-Item -Force (Join-Path $root "tools\BlockSetup.exe") (Join-Path $stage "tools\")
$toolsAssets = Join-Path $stage "tools\Assets"
New-Item -ItemType Directory -Force -Path $toolsAssets | Out-Null
Copy-Item -Force $stageAssets\* $toolsAssets

$openglDest = Join-Path $stage "build-win\runtime\opengl"
New-Item -ItemType Directory -Force -Path $openglDest | Out-Null
Copy-Item -Force (Join-Path $glOut "*") $openglDest -Recurse

# Minimal payload (addons README + default ini; shaders optional stub)
$payloadDest = Join-Path $stage "payload"
New-Item -ItemType Directory -Force -Path (Join-Path $payloadDest "addons") | Out-Null
Copy-Item -Force (Join-Path $root "payload\ReShade.ini") $payloadDest -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $root "payload\addons\README.txt") (Join-Path $payloadDest "addons\") -ErrorAction SilentlyContinue

# Lightweight readme for the zip
@"
Block $ver — Windows x64
========================

1. Unzip this folder anywhere.
2. Run BlockSetup.exe
3. Check every OpenGL viewer you use (Firestorm, Alchemy, Black Dragon,
   Cool VL, Singularity, Kokua, …) or Browse to a portable folder.
4. Install, launch the viewer normally, press Shift+B.

Needs: Windows 10/11 x64. .NET Framework 4.x (built into Windows).
GShade shader packs are optional — Setup wires them if installed under Program Files\GShade.

Linux: build the Vulkan layer from source (Meson). See docs/PLATFORMS.md.
macOS: coming soon — Metal rebuild planned. Not in this zip.

Viewers: docs/VIEWERS.md · Platforms: docs/PLATFORMS.md
Human craft only. https://github.com/ape-squared/Block
"@ | Set-Content (Join-Path $stage "README.txt") -Encoding UTF8

$zip = Join-Path $root "build-win\dist\Block-$ver-windows-x64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $stage -DestinationPath $zip -CompressionLevel Optimal

Write-Host "Packaged: $zip"
Get-Item $zip | Format-List FullName, Length, LastWriteTime
