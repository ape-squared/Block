# Contributing to Block

Open contributions are welcome. Read this before you open a PR.

## Ground rules

1. **Humans only.** See [docs/AI_POLICY.md](docs/AI_POLICY.md). AI-generated patches are not accepted.
2. **Be honest about viewers.** Do not claim Firestorm / Aurora / others work unless you tested them. See the website FAQ and README.
3. **Licence.** Contributions are under the project’s BSD-2-Clause terms (see [LICENSE](LICENSE)).
4. **No drive-by dependency dumps.** Keep PRs focused.

## Ways to help

- Fix bugs you can reproduce (attach logs: `ReShade.log`, Aurora logs, `BLOCK_LOG_FILE`)
- Improve Setup / OpenGL defaults for slow PCs
- Vulkan layer hardening (especially Aurora-safe instance creation)
- Docs, presets notes, and design polish that match `brand/` and `design/`
- Review PRs carefully — say what you ran

## Setup (Windows)

```bat
tools\build_reshade_block.ps1
tools\stage_block_runtime.ps1
tools\build_block_setup.ps1
tools\BlockSetup.exe
```

Vulkan layer (experimental):

```bat
tools\build_windows.bat
tools\stage_block_runtime.ps1
```

`third_party/reshade` is vendored (ignore `bin/` and `intermediate/`). After clone, build as above.

## Pull requests

- Branch from `main`
- One concern per PR when possible
- Describe **why**, what you tested, and which viewer / GPU
- Do not commit `build/`, `build-win/`, logs, or personal presets with secrets
- Match existing style; do not reformat the tree for taste

## Issues

- Search first
- Include OS, GPU, viewer version, and steps
- Crash → log excerpt, not a screenshot of a screenshot

## Code of conduct (short)

Be civil. No harassment. Disagreement about tech is fine; personal attacks are not. Maintainers may close or lock threads that go nowhere.
