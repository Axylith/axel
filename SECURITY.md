# Security Policy

## Reporting a Vulnerability

**Do NOT open a public issue for security vulnerabilities.**

If you discover a security vulnerability in Axylith, report it
privately via email:

**security@axylith.com**

Include:

- Description of the vulnerability
- Steps to reproduce
- Affected component (notebook, physics engine, .axl format, sync)
- Severity assessment (critical, high, medium, low)
- Any suggested fix (optional but appreciated)

## Response Timeline

| Action | Timeline |
|--------|----------|
| Acknowledgment of report | Within 48 hours |
| Initial assessment | Within 7 days |
| Fix development | Within 30 days for critical/high |
| Public disclosure | After fix is released |

We follow coordinated disclosure. We will not publicly disclose a
vulnerability until a fix is available. We ask that you do the same.

## Scope

The following are in scope:

- Any code in the axylith or axylith-physics repositories
- The .axl file format (buffer overflows, malformed input handling)
- The Vulkan renderer (GPU memory safety)
- The inference engine (model loading, weight parsing)
- The observer system (file system traversal, privilege escalation)
- Cloud sync protocol (auth bypass, data leakage)
- Encryption implementation (key management, cipher weaknesses)

The following are out of scope:

- Vulnerabilities in Vulkan drivers (report to NVIDIA/AMD/Intel)
- Vulnerabilities in the Linux kernel or X11
- Vulnerabilities in stb_truetype.h (report upstream)
- Social engineering attacks
- Denial of service via resource exhaustion on your own machine

## Recognition

Security researchers who responsibly disclose vulnerabilities will be:

- Credited in the release notes (unless anonymity is requested)
- Added to a SECURITY-ACKNOWLEDGMENTS.md file
- Eligible for our bug bounty program (when established)

## Supported Versions

| Version | Supported |
|---------|-----------|
| Latest release | Yes |
| Previous release | Security fixes only |
| Older releases | No |

## Security Design Principles

Axylith is designed with security as a core principle:

- **Zero network by default.** No telemetry, no phone-home,
  no external connections unless the user explicitly enables sync
  or cloud AI.
- **Minimal attack surface.** One external dependency (stb_truetype.h).
  No npm, no pip, no supply chain.
- **Memory safety.** AddressSanitizer and UBSanitizer run on every
  commit via CI/CD. Valgrind testing planned.
- **FP64 precision.** No floating-point shortcuts that could cause
  incorrect results in safety-critical computations.
- **Encryption planned.** XChaCha20-Poly1305 + Argon2id for data
  at rest. X25519 for sync encryption. TPM binding for enterprise.