# Light Block product branding for the ReShade-based OpenGL injector.
# Keep legal notices in LICENSE / THIRD_PARTY_LICENSES.md — UI stays product-first.

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$reshade = Join-Path $root "third_party\reshade"
if (-not (Test-Path $reshade)) { throw "Missing $reshade" }

Write-Host "Branding injector UI as Block..."

$rc2 = Join-Path $reshade "res\version.rc2"
$c = Get-Content $rc2 -Raw
$c = $c -replace 'VALUE "ProductName", "ReShade"', 'VALUE "ProductName", "Block"'
$c = $c -replace 'VALUE "CompanyName", "crosire"', 'VALUE "CompanyName", "Block"'
$c = $c -replace "crosire's ReShade post-processing injector for 32-bit", "Block post-processing injector for 32-bit"
$c = $c -replace "crosire's ReShade post-processing injector for 64-bit", "Block post-processing injector for 64-bit"
$c = $c -replace 'VALUE "OriginalFilename", "ReShade32.dll"', 'VALUE "OriginalFilename", "Block32.dll"'
$c = $c -replace 'VALUE "OriginalFilename", "ReShade64.dll"', 'VALUE "OriginalFilename", "Block64.dll"'
$c = $c -replace 'ReShade FX compiler', 'Block FX compiler'
Set-Content -Path $rc2 -Value $c -NoNewline

$gui = Join-Path $reshade "source\runtime_gui.cpp"
$g = Get-Content $gui -Raw
if ($g -notmatch 'ImGui::TextUnformatted\("BLOCK"\)') {
  $g = $g -replace 'ImGui::TextUnformatted\("ReShade " VERSION_STRING_PRODUCT\);', 'ImGui::TextUnformatted("BLOCK");'
  $g = $g -replace 'ImGui::TextUnformatted\("Block " VERSION_STRING_PRODUCT "  ·  based on ReShade"\);', 'ImGui::TextUnformatted("BLOCK");'
}
# Home + About: product-first (no upstream billboard)
$g = $g -replace '(?s)ImGui::TextUnformatted\("BLOCK"\);\s*if \(\(s_latest_version\[0\] > VERSION_MAJOR\).*?ImGui::Text\(_\("Visit %s for news, updates, effects and discussion\."\), "https://reshade\.me"\);\s*\}\s*ImGui::Spacing\(\);', "ImGui::TextUnformatted(`"BLOCK`");`r`n`t`t`tImGui::TextDisabled(`"Post-processing for OpenGL and Vulkan.`");`r`n`r`n`t`t`tImGui::Spacing();"
$g = $g -replace '(?s)ImGui::TextUnformatted\("BLOCK"\);\s*ImGui::SameLine\(ImGui::GetContentRegionAvail\(\)\.x - ImGui::CalcTextSize\("https://reshade\.me"\)\.x, ImGui::GetStyle\(\)\.ItemSpacing\.x\);\s*ImGui::TextLinkOpenURL\("https://reshade\.me"\);', "ImGui::TextUnformatted(`"BLOCK`");`r`n`tImGui::TextDisabled(`"Post-processing for OpenGL and Vulkan.`");"
$g = $g -replace 'ImGui::TextUnformatted\(_\("Developed and maintained by crosire\."\)\);\s*\r?\n\s*ImGui::TextUnformatted\(_\("This project makes use of several open source libraries, licenses of which are listed below:"\)\);', 'ImGui::TextUnformatted(_("Open-source components used under their licenses:"));'
$g = $g -replace 'if \(ImGui::CollapsingHeader\("ReShade", ImGuiTreeNodeFlags_DefaultOpen\)\)', 'if (ImGui::CollapsingHeader("License"))'
$g = $g -replace 'if \(ImGui::CollapsingHeader\("Block", ImGuiTreeNodeFlags_DefaultOpen\)\)', 'if (ImGui::CollapsingHeader("License"))'
$g = $g -replace "Welcome! Since this is the first time you start ReShade", "Welcome! Since this is the first time you start Block"
$g = $g -replace "ReShade is now installed successfully!", "Block is now installed successfully!"
$g = $g -replace 'This build of ReShade has only limited add-on functionality\.', 'This build of Block has only limited add-on functionality.'
$g = $g -replace 'This build of Block \(ReShade-based\) has only limited add-on functionality\.', 'This build of Block has only limited add-on functionality.'
$g = $g -replace 'Some add-ons were not loaded because this build of ReShade has only limited add-on functionality\.', 'Some add-ons were not loaded because this build of Block has only limited add-on functionality.'
$g = $g -replace 'Some add-ons were not loaded because this build of Block has only limited add-on functionality\.', 'Some add-ons were not loaded because this build of Block has only limited add-on functionality.'
Set-Content -Path $gui -Value $g -NoNewline

Write-Host "Done."
