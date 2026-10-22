# Helicoid development workspace

This workspace coordinates four independently buildable private repositories:
[core](https://github.com/Helicoid-Systems/core),
[server](https://github.com/Helicoid-Systems/server),
[macOS](https://github.com/Helicoid-Systems/macos), and
[Windows](https://github.com/Helicoid-Systems/windows).
The submodule entries record exact integration commits.

The shared Qt Manager source is at `apps/manager`. It is independently
configured with CMake and is not yet a separate Git submodule. Its target is
`HelicoidManager`; the source pins SARibbon and requires Qt 6.10.x and CMake
3.28+.

Clone with `--recurse-submodules` using an account with access to all four.
Each repository includes its own build instructions, lock/source inputs and CI.
The original local product plans and branding are preserved separately.

Run `scripts/verify.sh core`, `server`, `macos` or `windows-domain` with the
appropriate Rust, Xcode/Swift 6.2+ or .NET 10 toolchain on PATH. Mac apps package
with `apps/macos/scripts/build.sh`; Windows app compilation runs on Windows CI.

Build the shared Manager after installing a Qt 6.10.x desktop SDK, CMake 3.28
or newer, and the platform generator. Set `QT6_ROOT` to the platform-specific
Qt installation directory.

```sh
export QT6_ROOT=/path/to/Qt/6.10.0/macos
cmake --list-presets -S apps/manager
cd apps/manager
cmake --preset manager-macos-arm64-release
cmake --build --preset manager-macos-arm64-release
```

Use `manager-macos-x86_64-release` for Intel macOS or
`manager-windows-x64-release` in a Visual Studio 2022 developer environment.
On Windows, set `QT6_ROOT` to the matching MSVC Qt 6.10.x installation before
configuring.

The CMake package check rejects Qt versions outside 6.10.x. The first Manager
configure fetches the pinned SARibbon source. This
workspace has not yet built the target because no Qt 6.10 SDK is installed in
the current environment. The root `Shared Qt Manager` GitHub Actions workflow
builds and packages native macOS arm64, macOS x86_64, and Windows x64
development artifacts when Manager sources change.

For actual cross-repository development transport checks, build `hs-server`
and run `scripts/live-integration.sh /absolute/path/to/hs-server` with .NET 10
on PATH. On macOS this also compiles the real Swift client probe using Xcode.
The script creates an isolated disposable loopback database and exercises
health, bootstrap, login, scoped inventory, approval/revocation, denials and
logout. Credentials move only through subprocess stdin and are not printed.
The Linux/Windows-compatible Python HTTP smoke can run alone using
`scripts/server-smoke.py /absolute/path/to/hs-server`.

See [implementation evidence](docs/implementation-2026-10-10.md) and
[Windows test instructions](docs/windows-testing.md). This is a development
foundation. Native apps, loopback accounts, manager approval and scoped
inventory are built.
Deployment-bound v2 administrator MFA and Linux installer prerequisites exist
as isolated candidates; remote media/control, public TLS trust, installed
hosting/recovery and broader version 1 tools remain unfinished. The RustDesk
native proof is isolated in core's
engine lab and is not enabled in either app.

The isolated `modules/core/authority-lab` candidate passes 167 runtime tests and
four compile-fail checks for committed issuance, bounded credentials, replay
consumption, storage fault denials and separate manager-key possession.
Run `scripts/verify.sh authority-lab` with Rust on PATH. Issuance and endpoint
consumption use separate persistent stores. Real
authenticated transport, OS admission, independent rollback anchoring and
recovery cutover remain unproved. It is not enabled in the apps.

The corrected [Mac app archive download](https://github.com/Helicoid-Systems/macos/actions/runs/38085682009/artifacts/11682776126)
contains both Apple Silicon apps with verified complete ad-hoc signatures and
approved role-specific icon fallbacks. Unzip the inner role archive to retain
the app's executable permissions.
