---
status: accepted
date: 2026-10-09
---

# Native apps with a shared Rust engine

The user chose Swift for macOS, WinUI 3 / Windows App SDK for Windows, and explicitly accepted a shared Rust engine. Native code owns each platform's interface, accessibility, and OS integration; Rust shares remote desktop and secure networking machinery. This avoids duplicating the difficult transport/media work while preserving the requested platform experience. RustDesk itself remains a candidate pending the adoption gate in the product plan; choosing Rust does not commit us to its current application architecture.
