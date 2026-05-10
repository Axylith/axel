# Changelog

All notable changes to Axylith are documented here.

Format based on [Keep a Changelog](https://keepachangelog.com/).

---

## [Unreleased]

### Added
- X11 window creation with WM_DELETE_WINDOW support
- Vulkan 1.3 initialization (instance, surface, device, swapchain)
- Dynamic rendering with vkCmdBeginRendering/vkCmdEndRendering
- Graphics pipeline with vertex + fragment shaders
- First colored quad rendering (Tol palette teal)
- Vertex buffer with GPU memory allocation
- Alpha blending in pipeline
- Threaded Vulkan initialization (window at 0.3ms)
- Resource monitoring (CPU/RAM CSV output)
- X connection error handling (clean shutdown on WM kill)
- .axl binary format: Block (12B), Page (22B), Project (10B), Header (32B)
- Tiered timestamp encoding (minutes/hours/days in 2 bytes)
- 27/27 format tests passing
- CI/CD: GitLab CI + GitHub Actions + CircleCI
- Sanitizer builds (ASan + UBSan) on CircleCI
- Static analysis (clang-tidy) on CircleCI
- Dual compiler builds (GCC + Clang)
- SonarCloud integration
- AGPL v3 license
- CONTRIBUTING.md with CLA and mentorship program
- CODE_OF_CONDUCT.md with AI policy and three-strike system
- SECURITY.md
- GOVERNANCE.md
- FOUNDERS.md (template)
- funding.json and FUNDING.yml
- README.md with badges