@echo off
setlocal
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
if not defined VULKAN_SDK set "VULKAN_SDK=C:\VulkanSDK\1.4.363.0"
set "PATH=%VULKAN_SDK%\Bin;%APPDATA%\Python\Python314\Scripts;%PATH%"
cd /d "%~dp0.."
if not exist build-win (
  meson setup build-win --buildtype=debug || exit /b 1
) else (
  meson setup build-win --reconfigure --buildtype=debug || exit /b 1
)
meson compile -C build-win || exit /b 1
echo.
echo Built: build-win\src\hooks\block.dll
echo Layer: build-win\config\Block_x86_64.json
echo Enable with ENABLE_BLOCK=1 and VK_LAYER_PATH pointing at the JSON directory
endlocal
