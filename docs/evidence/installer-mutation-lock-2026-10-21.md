# Installer mutation lock evidence — 2026-10-21

## Implemented

The Linux installer adapter acquires a nonblocking exclusive `flock` on the
private root-owned `/run/lock/helicoid-installer.lock`. Install, resume,
removal, and the legacy apply/remove entry points hold it across their durable
state and host mutations. Competing operations return a conflict; process exit
releases the kernel lock so an interrupted journal can be resumed. The
versioned lifecycle contract documents this behavior.

## Local verification

- On `aarch64-apple-darwin`, the focused Unix `flock` test passed:
  `cargo test --locked --lib exclusive_lifecycle_lock_rejects_competitor_and_releases_on_drop -- --nocapture` (1 passed).
- `rustfmt --check --edition 2021 --config skip_children=true src/installer/adapter.rs` passed.
- `cargo check --locked --lib` passed on the macOS host.
- `cargo clippy --locked --all-targets -- -D warnings` passed on the macOS host.
- `git diff --check` passed for the server submodule.

## Not verified

- This host has only the `aarch64-apple-darwin` Rust target installed. The
  Linux-only `LinuxAdapter` path, including `/run/lock` ownership and mode
  checks, was not compiled or exercised on Ubuntu.
- The crate-wide `cargo fmt -- --check` reports formatting differences in
  other already-modified files (`auth_v2.rs`, `file_requests.rs`, `http.rs`,
  `lib.rs`, and `service.rs`); those unrelated files were left untouched.
- The test validates flock contention and release using separate open file
  descriptors in one process; it is not an installed-host concurrent CLI test.
