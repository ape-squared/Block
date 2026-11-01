# Security

## Supported versions

We accept reports against the latest release on [GitHub Releases](https://github.com/ape-squared/Block/releases) and against `main`.

## Reporting a vulnerability

Open a **private** security advisory on GitHub if available, or email the maintainers listed on the organization profile. Do not file a public issue for exploitable flaws in the injector or update path.

Include:

- Affected backend (OpenGL injector / Vulkan layer / Setup)
- Version or commit
- Reproduction steps
- Impact (crash, code exec, unexpected DLL load, …)

## Scope notes

Block loads into other processes (viewer / Vulkan apps). Treat third-party FX shaders and addons as untrusted input. We do not run a Block backend server; OTA checks fetch public `latest.json` from GitHub Pages.
