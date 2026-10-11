# Helicoid development workspace

This workspace coordinates four independently buildable private repositories:
[core](https://github.com/Helicoid-Systems/core),
[server](https://github.com/Helicoid-Systems/server),
[macOS](https://github.com/Helicoid-Systems/macos), and
[Windows](https://github.com/Helicoid-Systems/windows).
The submodule entries record exact integration commits.

Clone with `--recurse-submodules` using an account with access to all four.
Each repository includes its own build instructions, lock/source inputs and CI.
The original local product plans and branding are preserved separately.

Run `scripts/verify.sh core`, `server`, `macos` or `windows-domain` with the
appropriate Rust, Xcode/Swift 6.2+ or .NET 10 toolchain on PATH. Mac apps package
with `apps/macos/scripts/build.sh`; Windows app compilation runs on Windows CI.

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
