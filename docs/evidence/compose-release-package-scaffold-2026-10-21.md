# Linux Compose release artifact scaffold — 2026-10-21

## Status

The release artifact builder now stages a Linux amd64 Docker image archive
from the same hs-server and helicoid-server binaries used by the systemd
package. It normalizes the single-image Docker archive to a content-derived
local tag, renders the release Compose file, and places all output below
artifacts/compose/ so the existing signed-package builder includes it.

The lifecycle backend now has source implementations for reviewed Compose
plan, apply, resume, status, and removal, including resource ownership and
data-preserving recovery. The Docker image has not been built in this
environment and no signed package or Linux deployment was produced, so this
does not establish live Compose acceptance.

## Fresh local evidence

- bash -n modules/server/scripts/build-linux-artifact-dir.sh — passed.
- bash -n modules/server/scripts/build-linux-compose-artifacts.sh — passed.
- Python AST parse of modules/server/scripts/normalize-compose-image.py — passed.
- rustfmt --edition 2021 --check modules/server/src/installer/machine.rs — passed.
- cargo check --locked --manifest-path modules/server/Cargo.toml --bin hs-server — passed.
- The Linux amd64 TUI go build could not fetch its pinned Charm modules because proxy.golang.org did not resolve in this network-restricted environment; no TUI release binary was produced.
- A full cargo fmt --manifest-path modules/server/Cargo.toml -- --check reports pre-existing formatting differences in modules/server/tests/http_api.rs; that unrelated file was left untouched.
- docker buildx version reports Buildx 0.36.1 on Darwin. docker info --format '{{.ServerVersion}}' cannot connect because /Users/barni/.docker/run/docker.sock is absent.
- The lifecycle update separately passed an offline host-target library compile, targeted rustfmt, Bash syntax parsing, and changed-path whitespace checks. This macOS check does not compile the Linux-only adapter implementation; the Go TUI build and Docker lifecycle remain unverified.

No image archive export, package signing, Docker image load, Compose start,
Linux host install, or interruption/recovery workflow was observed.
