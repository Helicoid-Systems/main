# Linux TUI bounded service event diagnostics — 2026-10-22

The TUI now has a read-only `logs recent` command and an `l` action on the
Diagnostics and support screen. On Linux systemd installs it invokes only the
root-owned `journalctl` executable for `hs-server.service`, with a 15-minute
window, a 100-record limit, and a five-second collection deadline. Parsing is
bounded to 512 KiB total and 64 KiB per record. Only exact allowlisted static
message prefixes at the start of Rust's formatted tracing line are translated
to stable event codes. The parser recognizes its bounded timestamp, level, and
quoted-message prefix, then discards every suffix field and raw message.

For Compose, the container requests logs over the existing host control socket.
The host checks the durable installation record, project, container labels, and
signed image, then runs fixed Docker commands with five-second per-command
deadlines and a 256 KiB output cap. The host filters raw messages before they
cross the socket; the container receives only event codes, timestamps, and an
omission count. The control socket verifies the caller's UID, and Compose
severity is `unspecified` because Docker's merged logs do not preserve
systemd's priority field. No Docker-daemon socket is mounted in the container.

The schema-v2 support export remains capped at 64 KiB and includes only those
codes, UTC timestamps, severities, and fixed availability/omission metadata.

The support bundle is now schema version 2. It remains capped at 64 KiB, uses
mode `0600`, and does not overwrite an existing file. Audit-event export is
still unavailable. The Compose host-control path is implemented in source but
has not been exercised against an installed Linux deployment.

## Evidence

- `gofmt -w logs.go main.go support.go tui.go` completed in
  `modules/server/tui`.
- The default Go build cache under the user Library directory was denied by the
  workspace sandbox. Builds using the allowed cache path succeeded for the
  current macOS host:
  `GOCACHE=/private/tmp/helicoid-go-cache-20261022 go build -o /tmp/helicoid-server .`.
- The Linux x64 cross-build also succeeded:
  `GOCACHE=/private/tmp/helicoid-go-cache-20261022 CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -o /tmp/helicoid-server-linux .`.
- `cargo fmt --check` and
  `CARGO_TARGET_DIR=/private/tmp/hel-server-target cargo check --locked --offline`
  completed successfully for the current macOS target.

`cargo check` ran for the current macOS target. The Rust toolchain has only the
Apple Darwin target installed, so the Linux-only host socket handler was not
compiled in this environment. These are compile checks only. Neither journal
collector was exercised on an installed Linux deployment, and this does not
provide audit export, installed-service proof, or full core-v1 acceptance
evidence.
