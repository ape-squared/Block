# Contributing to Block

Open contributions welcome — from **humans**.

Block is post-processing that loads into other people’s processes. A bad patch
crashes a viewer mid-session. We expect contributors to understand what they
ship, to test it, and to own the diff.

Everything in the repository and on GitHub is written in **English**. You can
discuss ideas in whatever language you like; the PR itself stays English.

| Start here | |
|---|---|
| [docs/AI_POLICY.md](docs/AI_POLICY.md) | AI-generated patches are **not** accepted |
| [docs/VIEWERS.md](docs/VIEWERS.md) | Supported / expected viewers |
| [docs/PLATFORMS.md](docs/PLATFORMS.md) | Windows · Linux · macOS / Metal |
| [ARCHITECTURE.md](ARCHITECTURE.md) | OpenGL injector vs Vulkan layer |
| [docs/HANDBOOK.md](docs/HANDBOOK.md) | Repo map and product principles |
| [NOTICE.md](NOTICE.md) | Upstream credit (ReShade, vkShade, …) |
| [README.md](README.md) | Product overview and quick start |

---

## Ground rules

1. **Human authorship.** Read [docs/AI_POLICY.md](docs/AI_POLICY.md). No Copilot /
   ChatGPT / Claude / Cursor Agent / Gemini paste-jobs — disclosed or not.
2. **Honest compatibility.** Only claim a viewer or app you actually tested.
   “Works on Firestorm” means you launched Firestorm with your build.
3. **Licence.** Code lands under [BSD-2-Clause](LICENSE). When you vendor or
   port third-party code, update [NOTICE.md](NOTICE.md) and
   [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
4. **Focused PRs.** One concern when possible. No drive-by dependency dumps,
   no reformatting half the tree “while you were there.”
5. **Brand.** Logos, lime signal (`#D4FF3F`), and lockups live in `brand/` and
   `design/`. Do not invent a second visual language for the overlay or the site.

---

## Ways to help

High-value work, roughly in order of need:

| Area | Examples |
|------|----------|
| **Bugs you can reproduce** | Overlay crash, Setup mis-detect, wrong addon path — with log text |
| **OpenGL / Firestorm** | Slow-PC defaults, preset paths, Shift+B UX, compile times |
| **Setup** | Viewer discovery, install messaging, OTA banner copy |
| **Vulkan layer** | Hardening, CRT, clearer failure when the layer should not load |
| **Docs & site** | Handbook accuracy, Pages one-pager, `latest.json` notes |
| **Review** | Read a PR, say what you built and ran |

Feature ideas: use the [feature request form](https://github.com/ape-squared/Block/issues/new/choose).
Search existing issues first.

---

## Development environment (Windows)

Block targets **Windows 10/11 x64**.

### Prerequisites

- Git  
- Visual Studio 2022 **Build Tools** (MSVC) with C++ workload — required for the
  OpenGL injector (`tools/build_reshade_block.ps1`)  
- For the Vulkan layer: a Meson/Ninja toolchain as used by `tools/build_windows.bat`
  (see that script / Meson wrap setup if you are new to the tree)

### Clone

```bat
git clone https://github.com/ape-squared/Block.git
cd Block
```

### Build — OpenGL injector (primary)

```bat
powershell -ExecutionPolicy Bypass -File tools\build_reshade_block.ps1
powershell -ExecutionPolicy Bypass -File tools\stage_block_runtime.ps1
powershell -ExecutionPolicy Bypass -File tools\build_block_setup.ps1
tools\BlockSetup.exe
```

- Staged DLL: `build-win\hook\opengl32.dll` (also copied as D3D proxies for packaging)  
- Setup installs into a viewer folder you choose (Firestorm recommended)

### Build — Vulkan layer (experimental)

```bat
tools\build_windows.bat
tools\stage_block_runtime.ps1
```

Smoke test (do **not** point a production SL Vulkan viewer at this unless you
know what you are doing):

```bat
set ENABLE_BLOCK=1
set VK_LAYER_PATH=%CD%\build-win\runtime\vulkan
set VK_INSTANCE_LAYERS=VK_LAYER_BLOCK_post_processing_64
vkcube
```

### What not to commit

- `build/`, `build-win/`, `third_party/reshade/bin/`  
- Viewer logs (`ReShade.log`, crash dumps)  
- Local `ReShade.ini` with personal paths  
- Secrets, tokens, or private presets you do not own

---

## Where things live

| What | Where |
|------|--------|
| Vulkan layer sources | `src/` |
| OpenGL injector (ReShade, Block-branded) | `third_party/reshade/` |
| Build / Setup / configure scripts | `tools/` |
| Default ini, shader stubs, addon notes | `payload/` |
| Logos and marks | `brand/` |
| UI / web mockups (reference) | `design/` |
| GitHub Pages + OTA `latest.json` | `website/` |
| Handbook, AI policy | `docs/` |
| Product version | `VERSION` + `third_party/reshade/include/block_version.h` (**keep in sync**) |

See [ARCHITECTURE.md](ARCHITECTURE.md) for how the two backends relate.

---

## Testing before you open a PR

Minimum bar depends on what you touched:

| You changed… | Please verify |
|--------------|----------------|
| OpenGL injector / overlay | Build → install with Setup (or copy `opengl32.dll`) → launch Firestorm → **`Shift`+`B`** → exercise the path you changed (**not covered by CI UI tests**) |
| Setup | Run `BlockSetup.exe` against a throwaway or backup viewer folder; read the install log lines (**WinForms UI not in PR CI yet**) |
| Vulkan layer | `vkcube` (or your test app) with the env vars above |
| Docs / website | PR CI runs Playwright against `website/` (see `tests/website-ui/`); also open pages locally |
| OTA / version | Bump both `VERSION` and `block_version.h`; confirm banner logic still makes sense |

**Logs:** paste `ReShade.log` (OpenGL) or your layer log as **text** in the PR or issue. Prefer that over screenshots of screenshots.

**Viewers:** name the exact build (e.g. `Firestorm-Releasex64`) and GPU. If you did not test a viewer, write “not tested.”

---

## Pull requests

1. Branch from up-to-date `main`.  
2. Keep the diff focused.  
3. Fill in [.github/PULL_REQUEST_TEMPLATE.md](.github/PULL_REQUEST_TEMPLATE.md):
   - `Closes #…` / `Refs #…` / `No issue`  
   - What it is for (user’s point of view)  
   - What changed, by area  
   - Viewer / GPU tested  
   - Checklist — especially the **human-authored** confirmation  
4. Open the PR against `main`.  
5. Answer review comments with code or a clear “won’t do / follow-up issue.”

### Automated PR checks

Every pull request runs [`.github/workflows/pr-checks.yml`](.github/workflows/pr-checks.yml):

| Check | What it does |
|-------|----------------|
| Human authorship gate | PR body must confirm you wrote the patch (template checklist) |
| AI heuristic scan | Flags common LLM watermarks in added lines |
| Linux Vulkan build | Meson compile of the Vulkan layer |
| Doc path sanity | Required docs / site files still exist |
| Website UI (Playwright) | Smoke-tests home, download/`latest.json` CTA, docs/contribute/ai, mobile nav |

A green check does **not** replace “I ran Shift+B on my viewer.” Overlay and Setup UI are still manual.

**Local website UI tests** (optional, before opening a docs/site PR):

```bash
cd tests/website-ui
npm install
npx playwright install chromium
npm test
```

### Style

- Match neighbouring code. Do not reformat files you did not need to touch.  
- Overlay / Setup strings: plain English, Block brand voice — short, direct.  
- Comments explain **why**, not what the next line obviously does.

### Review expectations

Maintainers look for: human authorship, real testing notes, no untested
compatibility claims, and no accidental build artefacts. A green CI check (when
present) does not replace “I ran Shift+B on Firestorm.”

---

## Issues

Use [New issue](https://github.com/ape-squared/Block/issues/new/choose):

- **Bug report** — steps, expected / actual, backend, environment, logs  
- **Feature request** — problem, proposal, area; AI-policy checkbox  

Search first. One problem per issue when you can. Public attachments are
public — strip passwords and session tokens from logs.

---

## Releases (maintainers)

We follow [SemVer](https://semver.org/): `vMAJOR.MINOR.PATCH`. Before 1.0, a
feature bumps **minor**; a fix bumps **patch**.

1. Bump [`VERSION`](VERSION) and `third_party/reshade/include/block_version.h`  
2. Tag `vX.Y.Z` and push the tag  
3. CI refreshes [`website/latest.json`](website/latest.json) and opens a GitHub Release  
4. Setup and the overlay (`Shift`+`B`) pick up the banner from  
   https://ape-squared.github.io/Block-GPPI/latest.json  

Do not publish a tag whose `block_version.h` still says the previous number.

---

## Conduct

Be civil. Technical disagreement is fine; personal attacks and harassment are
not. Maintainers may close issues or PRs that go nowhere, restate rejected AI
policy, or demand untested “support” claims.

Questions about process belong on the issue or PR thread — keep them concrete.
