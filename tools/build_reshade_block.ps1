# Build ReShade as Block OpenGL/D3D injector (x64 Release) and stage under build-win/hook.
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$reshade = Join-Path $root "third_party\reshade"
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -products * -requires Microsoft.Component.MSBuild `
  -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) {
  $fallback = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
  if (Test-Path $fallback) { $msbuild = $fallback }
}
if (-not $msbuild) { throw "MSBuild not found" }

$env:Path = "C:\Windows\System32;C:\Windows\System32\WindowsPowerShell\v1.0;C:\Program Files\Git\bin;C:\Users\Shadow\AppData\Local\Programs\Python\Python314;C:\Users\Shadow\AppData\Local\Programs\Python\Python314\Scripts;" + $env:Path

& (Join-Path $PSScriptRoot "brand_reshade_as_block.ps1")

Push-Location $reshade
try {
  & $msbuild ReShade.sln /p:Configuration=Release /p:Platform=64-bit /m /v:minimal
  if ($LASTEXITCODE -ne 0) { throw "ReShade build failed" }
} finally {
  Pop-Location
}

$dll = Join-Path $reshade "bin\x64\Release\ReShade64.dll"
if (-not (Test-Path $dll)) { throw "Missing build output: $dll" }

$hook = Join-Path $root "build-win\hook"
New-Item -ItemType Directory -Force -Path $hook | Out-Null
Copy-Item $dll (Join-Path $hook "Block64.dll") -Force
foreach ($n in @("dxgi.dll", "d3d11.dll", "d3d12.dll", "opengl32.dll")) {
  Copy-Item $dll (Join-Path $hook $n) -Force
}

$glRuntime = Join-Path $root "build-win\runtime\opengl"
New-Item -ItemType Directory -Force -Path $glRuntime | Out-Null
Copy-Item $dll (Join-Path $glRuntime "opengl32.dll") -Force

$shadersDst = Join-Path $hook "reshade-shaders"
New-Item -ItemType Directory -Force -Path $shadersDst | Out-Null
Copy-Item (Join-Path $root "payload\reshade-shaders\*") $shadersDst -Recurse -Force
Copy-Item (Join-Path $root "payload\ReShade.ini") (Join-Path $hook "ReShade.ini") -Force

Write-Host "Staged Block64.dll + opengl32.dll (+ D3D proxies) into $hook"
Write-Host "Also: $(Join-Path $glRuntime 'opengl32.dll')"
