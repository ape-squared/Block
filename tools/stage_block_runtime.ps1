# Stage dual-backend runtime folders under build-win/runtime.
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")

$vkSrc = Join-Path $root "build-win\config"
$vkDll = Join-Path $root "build-win\src\hooks\block.dll"
$glSrc = Join-Path $root "build-win\hook"

$vkOut = Join-Path $root "build-win\runtime\vulkan"
$glOut = Join-Path $root "build-win\runtime\opengl"

New-Item -ItemType Directory -Force -Path $vkOut | Out-Null
New-Item -ItemType Directory -Force -Path $glOut | Out-Null

# --- Vulkan layer ---
if (-not (Test-Path $vkDll)) { throw "Missing Vulkan layer DLL: $vkDll (build with tools\build_windows.bat first)" }

Copy-Item -Force $vkDll (Join-Path $vkOut "block.dll")
$jsonIn = Join-Path $vkSrc "Block_x86_64.json"
if (-not (Test-Path $jsonIn)) {
  # Fallback: regenerate-relative from template semantics
  $jsonIn = Get-ChildItem (Join-Path $root "build-win") -Recurse -Filter "Block_x86_64.json" | Select-Object -First 1 -ExpandProperty FullName
}
$manifest = @{
  file_format_version = "1.0.0"
  layer = @{
    name = "VK_LAYER_BLOCK_post_processing_64"
    type = "GLOBAL"
    library_path = "block.dll"
    api_version = "1.3.0"
    implementation_version = "1"
    description = "Block post-processing (Vulkan)"
    functions = @{
      vkGetInstanceProcAddr = "vkShade_GetInstanceProcAddr"
      vkGetDeviceProcAddr = "vkShade_GetDeviceProcAddr"
    }
    enable_environment = @{ ENABLE_BLOCK = "1" }
    disable_environment = @{ DISABLE_BLOCK = "1" }
  }
}
($manifest | ConvertTo-Json -Depth 10) | Set-Content (Join-Path $vkOut "Block_x86_64.json") -Encoding utf8
Copy-Item -Force (Join-Path $root "config\Block.ini") (Join-Path $vkOut "Block.ini") -ErrorAction SilentlyContinue

# --- OpenGL injector ---
if (-not (Test-Path (Join-Path $glSrc "opengl32.dll"))) {
  throw "Missing OpenGL hook: $glSrc\opengl32.dll (run tools\build_reshade_block.ps1 first)"
}

Copy-Item -Force (Join-Path $glSrc "Block64.dll") $glOut
Copy-Item -Force (Join-Path $glSrc "opengl32.dll") $glOut
foreach ($n in @("dxgi.dll", "d3d11.dll", "d3d12.dll")) {
  $p = Join-Path $glSrc $n
  if (Test-Path $p) { Copy-Item -Force $p $glOut }
}
if (Test-Path (Join-Path $glSrc "reshade-shaders")) {
  robocopy (Join-Path $glSrc "reshade-shaders") (Join-Path $glOut "reshade-shaders") /E /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
}
if (Test-Path (Join-Path $glSrc "ReShade.ini")) {
  Copy-Item -Force (Join-Path $glSrc "ReShade.ini") $glOut
}

Write-Host "Staged:"
Write-Host "  Vulkan: $vkOut"
Write-Host "  OpenGL: $glOut"
