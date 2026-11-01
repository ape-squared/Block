# Block Setup — pick a Second Life / OpenSim viewer and install the OpenGL injector.
# Double-click BlockSetup.bat or: powershell -ExecutionPolicy Bypass -File tools\BlockSetup.ps1

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$runtimeGl = Join-Path $root "build-win\runtime\opengl"
$hookGl = Join-Path $root "build-win\hook"
$configureScript = Join-Path $PSScriptRoot "configure_gshade_support.ps1"

function Get-InjectorSource {
  foreach ($dir in @($runtimeGl, $hookGl)) {
    $dll = Join-Path $dir "opengl32.dll"
    if (Test-Path $dll) { return $dir }
  }
  return $null
}

function Find-Viewers {
  $names = @(
    "Firestorm-Releasex64", "FirestormOS-Releasex64", "Firestorm*",
    "SecondLife", "SecondLifeViewer", "SecondLife*",
    "Alchemy*", "Cool VL Viewer*", "Black Dragon*", "Singularity*", "Catznip*",
    "Radegast*"
  )
  $roots = @(
    ${env:ProgramFiles},
    ${env:ProgramFiles(x86)},
    (Join-Path $env:LOCALAPPDATA "Programs")
  ) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique

  $found = [System.Collections.Generic.List[object]]::new()
  $seen = @{}
  foreach ($base in $roots) {
    foreach ($pat in $names) {
      Get-ChildItem -Path $base -Directory -Filter $pat -ErrorAction SilentlyContinue | ForEach-Object {
        $exe = Get-ChildItem $_.FullName -Filter "*.exe" -ErrorAction SilentlyContinue |
          Where-Object { $_.Name -match "Firestorm|SecondLife|Alchemy|CoolVL|BlackDragon|Singularity|Catznip|Radegast|Viewer" } |
          Select-Object -First 1
        if (-not $exe) {
          $exe = Get-ChildItem $_.FullName -Filter "*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        }
        if ($exe -and -not $seen.ContainsKey($_.FullName)) {
          $seen[$_.FullName] = $true
          $injected = Test-Path (Join-Path $_.FullName "opengl32.dll")
          $found.Add([pscustomobject]@{
            Name     = $_.Name
            Path     = $_.FullName
            Exe      = $exe.Name
            Injected = $injected
          })
        }
      }
    }
  }
  $found | Sort-Object Name
}

function Install-BlockToViewer([string]$viewerDir) {
  $src = Get-InjectorSource
  if (-not $src) { throw "No built injector. Run tools\build_reshade_block.ps1 first." }

  $dllSrc = Join-Path $src "opengl32.dll"
  $dllDst = Join-Path $viewerDir "opengl32.dll"
  if (Test-Path $dllDst) {
    $bak = Join-Path $viewerDir ("opengl32.dll.pre-block-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
    Copy-Item -Force $dllDst $bak
    Write-Log "Backed up existing opengl32.dll -> $(Split-Path $bak -Leaf)"
  }
  Copy-Item -Force $dllSrc $dllDst
  Write-Log "Installed opengl32.dll ($((Get-Item $dllSrc).Length) bytes)"

  $shaderSrc = Join-Path $src "reshade-shaders"
  if (-not (Test-Path $shaderSrc)) { $shaderSrc = Join-Path $root "payload\reshade-shaders" }
  $shaderDst = Join-Path $viewerDir "reshade-shaders"
  if ((Test-Path $shaderSrc) -and -not (Test-Path $shaderDst)) {
    robocopy $shaderSrc $shaderDst /E /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
    Write-Log "Copied local reshade-shaders fallback"
  }

  & $configureScript -ViewerDir $viewerDir
  Write-Log "Wrote ReShade.ini (GShade paths + Shift+B + Block style)"
}

# --- UI ---
$form = New-Object System.Windows.Forms.Form
$form.Text = "Block Setup"
$form.Size = New-Object System.Drawing.Size(640, 520)
$form.StartPosition = "CenterScreen"
$form.Font = New-Object System.Drawing.Font("Segoe UI", 10)
$form.BackColor = [System.Drawing.Color]::FromArgb(24, 24, 22)
$form.ForeColor = [System.Drawing.Color]::FromArgb(245, 245, 240)
$form.MinimizeBox = $false
$form.MaximizeBox = $false
$form.FormBorderStyle = "FixedDialog"

$accent = [System.Drawing.Color]::FromArgb(212, 255, 63)
$panel = [System.Drawing.Color]::FromArgb(36, 36, 32)

$title = New-Object System.Windows.Forms.Label
$title.Text = "BLOCK"
$title.Font = New-Object System.Drawing.Font("Segoe UI Semibold", 22)
$title.ForeColor = $accent
$title.Location = New-Object System.Drawing.Point(20, 12)
$title.AutoSize = $true
$form.Controls.Add($title)

$sub = New-Object System.Windows.Forms.Label
$sub.Text = "Install the OpenGL injector into a viewer. Overlay: Shift+B"
$sub.ForeColor = [System.Drawing.Color]::FromArgb(180, 180, 170)
$sub.Location = New-Object System.Drawing.Point(22, 52)
$sub.AutoSize = $true
$form.Controls.Add($sub)

$list = New-Object System.Windows.Forms.ListView
$list.View = "Details"
$list.FullRowSelect = $true
$list.MultiSelect = $true
$list.CheckBoxes = $true
$list.Location = New-Object System.Drawing.Point(20, 88)
$list.Size = New-Object System.Drawing.Size(580, 200)
$list.BackColor = $panel
$list.ForeColor = $form.ForeColor
$list.BorderStyle = "FixedSingle"
[void]$list.Columns.Add("Viewer", 180)
[void]$list.Columns.Add("Path", 300)
[void]$list.Columns.Add("Status", 80)
$form.Controls.Add($list)

function Refresh-List {
  $list.Items.Clear()
  foreach ($v in (Find-Viewers)) {
    $item = New-Object System.Windows.Forms.ListViewItem($v.Name)
    [void]$item.SubItems.Add($v.Path)
    [void]$item.SubItems.Add($(if ($v.Injected) { "Installed" } else { "Ready" }))
    $item.Tag = $v.Path
    if ($v.Name -match "Firestorm-Release") { $item.Checked = $true }
    [void]$list.Items.Add($item)
  }
}

$btnRefresh = New-Object System.Windows.Forms.Button
$btnRefresh.Text = "Rescan"
$btnRefresh.Location = New-Object System.Drawing.Point(20, 298)
$btnRefresh.Size = New-Object System.Drawing.Size(90, 30)
$btnRefresh.FlatStyle = "Flat"
$btnRefresh.BackColor = $panel
$btnRefresh.ForeColor = $form.ForeColor
$btnRefresh.Add_Click({ Refresh-List })
$form.Controls.Add($btnRefresh)

$btnBrowse = New-Object System.Windows.Forms.Button
$btnBrowse.Text = "Browse…"
$btnBrowse.Location = New-Object System.Drawing.Point(120, 298)
$btnBrowse.Size = New-Object System.Drawing.Size(90, 30)
$btnBrowse.FlatStyle = "Flat"
$btnBrowse.BackColor = $panel
$btnBrowse.ForeColor = $form.ForeColor
$btnBrowse.Add_Click({
  $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
  $dlg.Description = "Select viewer install folder (contains the .exe)"
  if ($dlg.ShowDialog() -eq "OK") {
    $item = New-Object System.Windows.Forms.ListViewItem((Split-Path $dlg.SelectedPath -Leaf))
    [void]$item.SubItems.Add($dlg.SelectedPath)
    [void]$item.SubItems.Add($(if (Test-Path (Join-Path $dlg.SelectedPath "opengl32.dll")) { "Installed" } else { "Ready" }))
    $item.Tag = $dlg.SelectedPath
    $item.Checked = $true
    [void]$list.Items.Add($item)
  }
})
$form.Controls.Add($btnBrowse)

$logBox = New-Object System.Windows.Forms.TextBox
$logBox.Multiline = $true
$logBox.ScrollBars = "Vertical"
$logBox.ReadOnly = $true
$logBox.Location = New-Object System.Drawing.Point(20, 340)
$logBox.Size = New-Object System.Drawing.Size(580, 90)
$logBox.BackColor = $panel
$logBox.ForeColor = $form.ForeColor
$logBox.BorderStyle = "FixedSingle"
$form.Controls.Add($logBox)

function Write-Log([string]$msg) {
  $logBox.AppendText("$(Get-Date -Format "HH:mm:ss")  $msg`r`n")
}

$btnInstall = New-Object System.Windows.Forms.Button
$btnInstall.Text = "Install Block"
$btnInstall.Location = New-Object System.Drawing.Point(400, 298)
$btnInstall.Size = New-Object System.Drawing.Size(200, 30)
$btnInstall.FlatStyle = "Flat"
$btnInstall.BackColor = $accent
$btnInstall.ForeColor = [System.Drawing.Color]::FromArgb(20, 20, 18)
$btnInstall.Font = New-Object System.Drawing.Font("Segoe UI Semibold", 10)
$btnInstall.Add_Click({
  $targets = @($list.CheckedItems | ForEach-Object { $_.Tag })
  if (-not $targets -or $targets.Count -eq 0) {
    [System.Windows.Forms.MessageBox]::Show("Check at least one viewer (or Browse to add one).", "Block Setup") | Out-Null
    return
  }
  if (-not (Get-InjectorSource)) {
    [System.Windows.Forms.MessageBox]::Show("Build the injector first:`ntools\build_reshade_block.ps1", "Block Setup") | Out-Null
    return
  }
  $btnInstall.Enabled = $false
  try {
    foreach ($t in $targets) {
      Write-Log "Installing -> $t"
      Install-BlockToViewer -viewerDir $t
      Write-Log "Done."
    }
    Write-Log "Launch the viewer, then press Shift+B. Browse presets from gshade-presets\ (author folders)."
    Refresh-List
  } catch {
    Write-Log "ERROR: $_"
    [System.Windows.Forms.MessageBox]::Show("$_", "Block Setup") | Out-Null
  } finally {
    $btnInstall.Enabled = $true
  }
})
$form.Controls.Add($btnInstall)

$hint = New-Object System.Windows.Forms.Label
$hint.Text = "Uses GShade shader pack if installed under Program Files\GShade. Presets: viewer\gshade-presets\"
$hint.ForeColor = [System.Drawing.Color]::FromArgb(140, 140, 130)
$hint.Location = New-Object System.Drawing.Point(22, 438)
$hint.Size = New-Object System.Drawing.Size(580, 40)
$form.Controls.Add($hint)

$src = Get-InjectorSource
if ($src) { Write-Log "Injector ready: $src\opengl32.dll" }
else { Write-Log "Injector not built yet — run tools\build_reshade_block.ps1" }

Refresh-List
[void]$form.ShowDialog()
