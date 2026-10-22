# Compose preflight runtime diagnostics

## Current lifecycle implementation

- The Linux command adapter checks the fixed `/usr/bin/docker` executable, a reachable Docker Engine, and the Compose plugin with bounded commands and validated version output.
- Lifecycle v2 preflight returns Engine and plugin versions; both are included in the reviewed plan digest and rechecked before apply or resume.
- Compose planning uses signed offline artifacts, explicit bind mounts, and the reviewed network and storage configuration.
- Apply journals the operation-specific image tag, project startup, runtime configuration, and staged TLS files. Status verifies operation labels and image identity without claiming bootstrap readiness.
- Removal uses a separate write-ahead journal, preserves the shared content-tagged image, and checks the original install identity before removing containers, networks, or the private TUI identity.
- Missing or mismatched runtime prerequisites fail closed. Compose updates, encrypted backups, restore, cancellation, and public-edge provisioning remain unavailable.

## Earlier pre-lifecycle verification

- `cargo test --manifest-path modules/server/Cargo.toml --test installer`: 57 passed.
- `cargo fmt --manifest-path modules/server/Cargo.toml -- --check`: passed.
- Suite attempts exposed nine loopback/TLS cases blocked when this sandbox denies socket binds with `EPERM`: one health test and eight TLS tests.
- The complete Rust suite passes when only those nine socket-dependent tests are skipped; the dedicated TLS target also passes its remaining 22 tests with its eight bind-dependent cases skipped.
- `gofmt` ran on the TUI JSON pass-through change. `go test ./...` could not fetch uncached Charm modules because the sandbox cannot resolve `proxy.golang.org`.

## Limits

Those earlier tests exercised the injected adapter and lifecycle guards; they did not prove Docker availability or a Compose deployment. The current source implementation is compile-checked below, but Linux-host and live Docker acceptance remain unverified.

## Current implementation checks

- `cargo check --offline --locked --lib --target-dir /tmp/helicoid-cargo-check` — passed.
- `rustfmt --edition 2021 --check` on the installer lifecycle files — passed.
- `bash -n modules/server/scripts/install-linux.sh` — passed.
- `git diff --check` on the changed installer, TUI, shell, and Compose paths — passed.
- No test suite was run for this implementation pass.
- At this earlier checkpoint, the Go TUI source was formatted but its build was unverified because pinned Charm dependencies were not cached.

No signed image archive, Linux deployment, Docker runtime, interruption/recovery workflow, or live install/remove cycle was observed. A successful compile is source-level evidence only.

## 2026-10-21 Compose build-context correction

The Compose build now supplies `modules/core` as the named `hs-core` build
context and the Rust build stage copies its manifest and `src` into the
relative location required by `hs-server`'s `path = "../core"` dependency.
The previous Docker build context exposed only `modules/server`, so the
Dockerfile could not resolve that dependency.

`docker compose config` passed and resolved the contexts to
`modules/server` and `modules/core`. `git diff --check` passed for the Compose
file and Dockerfile. `docker compose build server` could not run: the default
Buildx state path was unwritable, and a scratch Buildx retry then found no
Docker Engine socket at either the configured Desktop path or
`/var/run/docker.sock`. This corrects the source context but does not prove a
Docker image build or deployed Compose lifecycle; signed image packaging and
journaled Compose installation remain unimplemented.

## 2026-10-22 lifecycle continuation

The preceding build-context snapshot described the state at that checkpoint.
Since then, lifecycle v2 source has added Compose capability reporting, signed
offline-image plan/apply, operation-labeled status and resume, and journaled
removal. The live contract is updated in
[`installer-lifecycle-v2.md`](../../modules/server/contracts/installer-lifecycle-v2.md).
The Rust library check and shell syntax check recorded above are source checks;
no Docker image build or Linux install/remove recovery was observed. The Go TUI
source now rejects nonterminal removal results instead of showing them as
completed.

## 2026-10-22 Go TUI build

- `GOMODCACHE=/private/tmp/helicoid-go-modcache GOCACHE=/private/tmp/helicoid-go-build go build -o /private/tmp/helicoid-server-tui .` — passed.
- The first build exposed missing JSON fields in the Go lifecycle-capabilities type; the full v2 capability shape is now represented and the fresh build passes.
- `gofmt -d tui.go backend.go` — clean. No tests were run.

This is a Go compile check only. It does not demonstrate TUI interaction,
Compose runtime behavior, or an installed Linux lifecycle.
