# Linux storage configuration evidence — 2026-10-20

## Scope

This evidence covers the Linux systemd installer path for reviewed custom data
and backup directories. It does not claim completion of the Helicoid core v1
plan or acceptance on a target Linux host.

## Implemented

- V2 installer review binds normalized storage paths to its canonical plan and
  generated systemd unit.
- Preflight checks free space against the selected data path and refuses an
  existing selected storage root before the installer can change its owner or
  permissions.
- Apply assigns the service account to product-owned storage directories;
  removal reads the durable configuration and preserves data by default.
- The Go installer CLI and Gum/plain shell flow pass custom paths through the
  reviewed request. The bootstrap helper gets its database path from the
  durable install record.
- TUI status and removal discover the recorded configuration before calling
  the lifecycle backend.

## Local verification

- `cargo test --locked --test installer`: 54 passed.
- `cargo test --locked --all-targets` with the eight TLS socket checks and
  one loopback-socket check skipped: 320 passed, 10 filtered. Those checks
  could not bind loopback sockets because this sandbox returns
  `Operation not permitted` before the server is exercised.
- `cargo clippy --locked --all-targets -- -D warnings`: passed.
- `cargo fmt --all -- --check`, `bash -n scripts/install-linux.sh`,
  `gofmt -d` on touched Go files, and `git diff --check`: passed.
- `cargo run --locked -- installer configuration-v2 --help`: confirmed the
  read-only configuration command is registered.

## Not verified here

- `go test ./...` could not download the pinned Charm modules because DNS for
  `proxy.golang.org` is unavailable. Go source formatting passed; type checking
  and Go tests remain unverified.
- Qt Manager CMake configuration could not find Qt 6.10 development files.
- No systemd host was modified. Install, resume, ownership, and removal were
  exercised with the in-memory installer adapter only.
- LAN/TLS deployment, Compose, backup/restore, file jobs, endpoint admission,
  screen capture, media delivery, and authorized preview remain unavailable or
  unverified.
