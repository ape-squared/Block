# Build BlockSetup.exe — branded WinForms wizard (matches design/ui installer).
# Uses csc.exe so no .NET 4.8 targeting pack / SDK is required.
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$src = Join-Path $PSScriptRoot "setup\BlockSetup.cs"
$outDir = Join-Path $root "build-win\setup"
$assets = Join-Path $PSScriptRoot "setup\Assets"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# Refresh assets from brand/
$brand = Join-Path $root "brand"
if (Test-Path $brand) {
  New-Item -ItemType Directory -Force -Path $assets | Out-Null
  foreach ($pair in @(
    @("01-logos\lockup\block-lockup-on-dark-240.png", "lockup.png"),
    @("01-logos\mark\block-mark-128.png", "mark.png"),
    @("02-app-icon\block.ico", "block.ico"),
    @("02-app-icon\block-app-icon-32.png", "title-icon.png")
  )) {
    $srcA = Join-Path $brand $pair[0]
    $dstA = Join-Path $assets $pair[1]
    try { Copy-Item -Force $srcA $dstA -ErrorAction Stop }
    catch { Write-Host "WARN: could not refresh $($pair[1]) (in use) — keeping existing" }
  }
}

$csc = Join-Path $env:WINDIR "Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if (-not (Test-Path $csc)) { $csc = Join-Path $env:WINDIR "Microsoft.NET\Framework\v4.0.30319\csc.exe" }
if (-not (Test-Path $csc)) { throw "csc.exe not found" }

$out = Join-Path $outDir "BlockSetup.exe"
$ico = Join-Path $assets "block.ico"
$win32icon = if (Test-Path $ico) { "/win32icon:`"$ico`"" } else { "" }

$args = @(
  "/nologo", "/target:winexe", "/optimize+", "/platform:anycpu",
  "/reference:System.dll", "/reference:System.Drawing.dll", "/reference:System.Windows.Forms.dll",
  "/out:$out"
)
if ($win32icon) { $args += $win32icon }
$args += $src

& $csc @args
if ($LASTEXITCODE -ne 0) { throw "csc failed" }

# Ship Assets beside the exe so lockup/icon resolve at runtime
$outAssets = Join-Path $outDir "Assets"
New-Item -ItemType Directory -Force -Path $outAssets | Out-Null
Copy-Item -Force (Join-Path $assets "*") $outAssets

Copy-Item -Force $out (Join-Path $root "tools\BlockSetup.exe")
$toolsAssets = Join-Path $root "tools\Assets"
New-Item -ItemType Directory -Force -Path $toolsAssets | Out-Null
Copy-Item -Force (Join-Path $assets "*") $toolsAssets

Write-Host "Built: $out"
Write-Host "Also:  $(Join-Path $root 'tools\BlockSetup.exe')"
