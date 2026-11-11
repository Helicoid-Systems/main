# RustDesk source/build baseline, 10 October 2026

This document retains the earlier source/inert-session baseline. Later ABI v2,
owned-buffer and actual callback work is tracked in [current implementation
evidence](implementation-2026-10-10.md) and core's engine-lab reports. Historical
test counts and synchronous-drain statements below apply to that earlier patch.
They do not describe current bridge acceptance or a passed route gate.

Exact upstream checkout: `001b0e96173cea4e49f9f7a67bea95a177dd1de3`.
`hbb_common` submodule: `229b904508364c8997aad0fb5af57effac859f60`.
The complete upstream Cargo.lock resolves 60 Git package entries to commits.
The source exports cdylib, staticlib and rlib, with desktop Sciter code still
compiled independently of its optional Flutter path.

The coordinator ran `cargo check --locked --lib` successfully on macOS arm64
with Rust 1.99.0 and isolated native dependencies. The check reported 123
upstream warnings. Dependencies were built using vcpkg revision
`9e593bb18ea69cc5095e012465dcd675a822ed0d`, the pinned source overlay ports,
NASM 2.16.03 and pkgconf 3.0.3: aom 3.14.1, libvpx 1.15.2, libyuv 1857,
opus 1.5.2 and libjpeg-turbo 3.2.0. No global shell/toolchain configuration
was changed.

This verifies source compilation feasibility. It does not pass RD02 or adopt
the engine. An initial linked C harness tested an added lifecycle wrapper,
but that wrapper only stored a peer string and its own state. The coordinator
rejected interpreting this as initialization of RustDesk's real viewer.
The reviewed lab retains that distinction and the next real-session seam.

Required remaining proof includes a real UI-independent session/handler,
owned bounded frames and callback drain, authenticated endpoint admission,
console-session selection, input cleanup, redistribution inventory, and
installed platform testing. Neither native application enables remote media
or input from the lab. A successful version call or source build cannot
substitute for these gates.

Primary source pins: [Cargo.toml](https://github.com/rustdesk/rustdesk/blob/001b0e96173cea4e49f9f7a67bea95a177dd1de3/Cargo.toml),
[library module](https://github.com/rustdesk/rustdesk/blob/001b0e96173cea4e49f9f7a67bea95a177dd1de3/src/lib.rs),
[session interface](https://github.com/rustdesk/rustdesk/blob/001b0e96173cea4e49f9f7a67bea95a177dd1de3/src/ui_session_interface.rs),
and [license](https://github.com/rustdesk/rustdesk/blob/001b0e96173cea4e49f9f7a67bea95a177dd1de3/LICENCE).

## Reviewed inert Session seam

The corrected engine lab constructs and drops a real RustDesk
`Session<NativeHandler>` through an explicit `hs-native` feature. Its pure
constructor avoids the upstream defaults that read persisted user config.
Fresh macOS arm64 check/test/build and the linked C harness passed: nine Rust
tests and 45 C checks, disabled keyboard/file/clipboard fields, null media
output and process-local TCP listener count 0→0. The retained patch applies to
a fresh local clone at the exact upstream and hbb_common pins, with matching
SHA-256 hashes of all four resulting files. Wrong source, submodule and lock
pins are refused.

Close tears down the owned Session even when its generation cannot increment.
Drain remains synchronous; no async callback cancellation or real media/input
is proved. Connect and observation exports return unavailable. Full RD02 and
B03 remain incomplete. Exact source license, patch, header generator, harness,
commands and results live in core's `engine-lab/`.

The Intel macOS proof now also compiles the real `hs-native` library and links
the C harness from a fresh pinned checkout. Both artifacts are x86_64 Mach-O,
with the same 18 exports and no Sciter/Flutter dynamic dependencies. Intel
test and harness execution remain unverified: this Apple Silicon Mac has no
Rosetta installation. Compile/link evidence does not establish Intel runtime
behavior. The retained source/header drift check also passes independently.
