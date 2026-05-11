# Axylith

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![Vulkan 1.3](https://img.shields.io/badge/Vulkan-1.3-red.svg)](https://www.vulkan.org/)
[![Zero Dependencies](https://img.shields.io/badge/Dependencies-0-brightgreen.svg)](#)

A local-first computational research workbench. Notebook, physics engine, FP64 computation, and local AI in a single C++ binary. Zero external dependencies. Offline by default.

**Axylith replaces around $7,000/year of proprietary tooling with a single free binary.**

---

## What Axylith is

Axylith combines note-taking, verified computation, physics simulation, and AI annotation into one offline C++ binary. Every number is computed in FP64 and traceable to its source. Every AI output is grounded in local content, not hallucinated. Your data never leaves your machine unless you choose to sync it.

It is not a note-taking app. It is an IDE for thinking.

## Why this exists

Researchers and students are locked into proprietary tools that cost thousands of dollars a year and depend on license servers that go down. In May 2025, a ransomware attack took MATLAB offline for five million users for over a week. Students couldn't submit theses. Researchers couldn't open their own files. At least one person ended up pirating software they had already paid for because the license server was unreachable.

Axylith eliminates that dependency. No license server, no internet requirement, no subscription gating any feature. One binary, runs offline, forever.

## Key technical decisions

- **Language.** C++20 with one external header (`stb_truetype.h`). Everything else is written from scratch.
- **Rendering.** Vulkan 1.3 with dynamic rendering — not Electron, not a web view.
- **File format.** Custom `.axl` binary format, 12-byte blocks, two-stage compression (dictionary + static Huffman). An entire research career fits in roughly 9.8 MB.
- **AI.** Custom inference engine with hand-written SIMD assembly (SSE4.2, AVX2, AVX-512, NEON). Runs locally on consumer hardware.
- **Computation.** FP64 recursive-descent evaluator. Every number is computed, never hallucinated.
- **Windowing.** Direct OS APIs (X11, Win32, Cocoa). No GLFW, no SDL.
- **Observation.** Watches file changes, terminal output, and render results, and correlates them with your notes automatically.

## Current progress

- 27/27 tests passing on the `.axl` binary format.
- Vulkan 1.3 initialization through swapchain creation.
- Threaded init — window appears in 0.3 ms, Vulkan loads in the background.
- Per-session CPU, RAM, and GPU resource tracking.
- CI across three providers (GitLab CI, GitHub Actions, CircleCI).
- AddressSanitizer and UBSanitizer builds, plus clang-tidy static analysis.

## Building

```bash
git clone https://github.com/Axylith/axel.git
cd axel
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./axylith
```

Requires a C++20 compiler (GCC 13+ or Clang 16+), the Vulkan SDK, and X11 dev headers.

```bash
# Ubuntu / Debian
sudo apt install g++ cmake libvulkan-dev libx11-dev vulkan-validationlayers

# Arch
sudo pacman -S gcc cmake vulkan-devel libx11 vulkan-validation-layers
```

## Running tests

```bash
cd build
ctest --output-on-failure
```

## Project structure

```
src/
  main.cpp           — entry point, event loop, threaded init
  window.cpp/.h      — X11 windowing
  vulkan_init.cpp/.h — Vulkan instance, surface, GPU, device
  swapchain.cpp/.h   — swapchain creation and image views
  renderer.cpp/.h    — command pool, sync objects, render loop
  monitor.h          — CPU / RAM / GPU resource tracking
  types.h            — shared structs, timer, format types
tests/
  axl_test.cpp       — .axl format tests (27/27 passing)
```

## Repositories

| Repository | Description | License |
| --- | --- | --- |
| [axylith](https://github.com/Axylith/axel) | Notebook application | AGPL v3 |
| [axylith-physics](https://gitlab.com/axylith/axylith-physics) | Physics and simulation engine | AGPL v3 |

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request. All contributions require agreement to our CLA. First-time contributors are welcome — look for issues labeled `good first issue` or `mentored`.

If you find a security issue, please email security@axylith.com rather than filing a public issue.

## License

Axylith is licensed under **AGPL v3**. See [LICENSE](LICENSE) for full text.

The software is free for individuals, students, and researchers. **Commercial licenses** are available for organizations that cannot comply with AGPL. Contact founders@axylith.com.

---

## Founding team

Axylith is built by a small founding team based out of Calgary. We're looking for early users. Reach us at founders@axylith.com.

<!-- Add founder names, roles, and links here when ready to make public -->

## Powered by

[![Snyk](https://img.shields.io/badge/Security-Snyk-4C4A73?logo=snyk)](https://snyk.io/)
[![SonarCloud](https://img.shields.io/badge/Quality-SonarCloud-F3702A?logo=sonarcloud)](https://sonarcloud.io/)
[![Cloudflare](https://img.shields.io/badge/Edge-Cloudflare-F38020?logo=cloudflare)](https://www.cloudflare.com/)
[![GitLab](https://img.shields.io/badge/CI-GitLab-FC6D26?logo=gitlab)](https://gitlab.com/)
[![CircleCI](https://img.shields.io/badge/CI-CircleCI-343434?logo=circleci)](https://circleci.com/)

## Sponsors and backers

Axylith is supported by the community. If the project is useful to you, please consider [sponsoring on GitHub](https://github.com/sponsors/Axylith) or [backing us on Open Collective](https://opencollective.com/axylith).

<a href="https://opencollective.com/axylith"><img src="https://opencollective.com/axylith/sponsors.svg?width=890" /></a>
<a href="https://opencollective.com/axylith"><img src="https://opencollective.com/axylith/backers.svg?width=890" /></a>
