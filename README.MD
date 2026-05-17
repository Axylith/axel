<div align="center">

# Axylith

**A native research notebook. Single C++ binary. No Electron, no telemetry, no license server.**

[Documentation](https://docs.axylith.com) · [File Format](https://docs.axylith.com/format) · [Strategy](https://github.com/Axylith/axle/blob/development/changelog.md) · [Sponsor](https://github.com/sponsors/Axylith)

[![CodeQL](https://github.com/Axylith/axle/actions/workflows/codeql.yml/badge.svg)](https://github.com/Axylith/axle/actions/workflows/codeql.yml)
[![Snyk](https://snyk.io/test/github/Axylith/axle/badge.svg)](https://snyk.io/test/github/Axylith/axle)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)

</div>

---

Axylith is a research notebook implemented as a single native binary. The current build is a text editor with multi-line input, cursor movement, atomic save/load to a versioned file format, MTSDF text rendering through Vulkan 1.3, and a real-time HUD for input and frame latency.

The eventual goal — described in the [strategy document](https://github.com/Axylith/axle/blob/development/changelog.md) — is a computational research workbench that combines notes, deterministic numeric evaluation, physics simulation, and locally-run AI annotation. That product does not exist yet. This repository is the starting point.

## Status

> **Pre-V1.** The editor is usable for typing, navigating, and saving plain UTF-8 text. None of the higher-level features (computation, physics, AI) have been implemented.

What currently works:

- Multi-line text editor with full cursor movement (arrows, Home/End, vertical navigation)
- UTF-8 input via X11 input methods (correct Shift, AltGr, dead-key, and multibyte handling)
- Atomic save and load through a 16-byte-headered `.axl` format, with plain-text fallback for any other UTF-8 file
- MTSDF text rendering via a custom Vulkan 1.3 pipeline — resolution-independent, sub-pixel-accurate
- On-screen HUD showing live frame time and keystroke-to-submit input latency
- CI matrix across GCC 12, GCC 13, and Clang 17, with AddressSanitizer, UBSan, ThreadSanitizer, CodeQL, Snyk, and SonarCloud on every push

What is deliberately not yet implemented:

- Text selection · scroll viewport · mouse positioning · line wrap · find/replace · syntax highlighting
- The computational layer (FP64 evaluator, symbolic math)
- The physics engine
- Local AI inference
- Wayland, macOS, and Windows backends (Linux/X11 only for now)

The roadmap to V1 is in the [strategy document](https://github.com/Axylith/axle/blob/development/changelog.md).

## Build

Linux only at present. Verified on Ubuntu 24.04, Arch, and Fedora.

```bash
git clone https://github.com/Axylith/axle.git
cd axle
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/axylith
```

Build dependencies (Ubuntu/Debian):

```bash
sudo apt install -y \
    build-essential cmake ninja-build \
    libvulkan-dev vulkan-validationlayers \
    libx11-dev libxext-dev \
    glslc
```

For Arch and Fedora equivalents, plus a 5-minute walkthrough including first save and load, see [docs/quickstart](https://docs.axylith.com/quickstart).

## Tests

```bash
cd build
ctest --output-on-failure
```

The test suite exercises the on-disk format. Sanitizers run in CI on every push.

## Design principles

These are the constraints the project has committed to. They are intended as load-bearing — features that violate them won't be merged.

- **One native binary.** No Electron, no embedded browser, no JavaScript runtime. The build produces a stripped ELF measured in megabytes.
- **One external dependency.** `stb_truetype.h`, header-only. Everything else (Vulkan, X11, libc) is a system library.
- **Local-first.** Files on disk are the source of truth. No cloud account is required to use any feature. Network access is opt-in, never default.
- **Plain UTF-8 with a small header.** The `.axl` format is readable in `cat`, diffable in `git`, recoverable without Axylith installed. Compression and encryption are not in v1.
- **Honest latency.** Input latency and frame time are displayed in the HUD by code anyone can read. Numbers are measured, not estimated.

## File format

`.axl` is a 16-byte header followed by raw UTF-8 content. Files without the header are loaded as plain text. The format is specified in [docs/format](https://docs.axylith.com/format).

```
$ od -An -tx1 -N 16 untitled.axl
 41 58 4c 00  01 00  00 00  00 00 00 00 00 00 00 00
   A  X  L \0   v1     reserved (zeros)
$ tail -c +17 untitled.axl
hello world
```

A v2 structured format (`AXLE` magic, 32-byte header) is specified and unit-tested but not yet emitted by the editor. Readers will support both before writers switch.

## License

Axylith is licensed under the [GNU Affero General Public License v3](LICENSE). The editor binary, the file format, and all included subsystems are free software.

A commercial license is available for organizations that cannot comply with AGPL. Contact `founders@axylith.com`.

The companion [physics engine repository](https://github.com/Axylith/axylith-physics) (when published) will use BSL-1.1 with a conversion to Apache-2.0. See the [strategy document](https://github.com/Axylith/axle/blob/development/changelog.md) for the full licensing rationale.

## Contributing

Contributions are welcome. The active development line is the `development` branch; `main` tracks released versions.

Before opening a pull request, please read [CONTRIBUTING.md](CONTRIBUTING.MD) and the [Code of Conduct](CODE_OF_CONDUCT.MD). A CLA is required.

First-time contributors: look for issues labeled `good first issue` or `mentored`.

## Acknowledgements

Axylith is built by [Dev Bhatt](https://devbhatt.dev) and contributors. The project uses font atlas data derived from [JetBrains Mono](https://www.jetbrains.com/lp/mono/) and the MTSDF baking technique described by Viktor Chlumský.

---

<div align="center">

If you find this project worth supporting, please consider [sponsoring on GitHub](https://github.com/sponsors/Axylith) or becoming a backer on [Open Collective](https://opencollective.com/axylith).

</div>
