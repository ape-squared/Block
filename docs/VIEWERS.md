# Supported viewers

Block’s **supported** path is the **OpenGL injector** (`opengl32.dll` via Block Setup) on Windows. Press **Shift+B** in-world for the overlay.

We only mark a viewer **supported** when it has been exercised with Setup + Shift+B. Others may work the same way (same OpenGL drop-in) but are listed as **expected** until confirmed.

| Viewer | Backend | Status | Notes |
|--------|---------|--------|--------|
| **Firestorm** | OpenGL | Supported | Primary test target |
| **Alchemy** | OpenGL | Supported | Detected by Setup; install via wizard |
| **Black Dragon** | OpenGL | Supported | Setup replaces GShade symlink carefully |
| **Second Life (official)** | OpenGL | Expected | Same injector pattern |
| **Cool VL Viewer** | OpenGL | Expected | Detected by Setup |
| **Singularity** | OpenGL | Expected | Detected by Setup |
| **Catznip** | OpenGL | Expected | Detected by Setup |
| **Kokua** | OpenGL | Expected | Detected by Setup |
| **Dolphin / Dayturn / Niran’s** | OpenGL | Expected | Detected when folder names match |
| **Aurora Viewer** | Vulkan | Experimental | Uses `block.dll` layer + launch bat — not OpenGL |

## Install

1. Download the Windows zip from [Releases](https://github.com/ape-squared/Block/releases).  
2. Run **BlockSetup.exe**.  
3. Check every OpenGL viewer you use (or browse to a portable folder).  
4. Launch the viewer normally → **Shift+B**.

If Setup misses a portable install, use **Browse…** and select the folder that contains the viewer `.exe`.

## Honest compatibility

Do not claim a viewer “works” in a PR unless you launched that viewer with your build. See [AI_POLICY.md](AI_POLICY.md) and [CONTRIBUTING.md](../CONTRIBUTING.md).
