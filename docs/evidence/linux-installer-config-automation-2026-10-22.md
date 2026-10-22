# Linux installer configuration mode — 2026-10-22

## Change

`modules/server/scripts/install-linux.sh` now supports first-time unattended
setup from a strict JSON configuration using `--non-interactive --config`.
The root-only path accepts a root-owned mode-0600 configuration, rejects
symlinked paths and malformed/duplicate/unknown fields, stages values as
NUL-delimited data rather than shell code, and passes the selected package,
deployment, network, and storage settings through the existing Rust
`installer plan` command. Without `--confirm`, it retains the protected
reviewed request for inspection. With `--confirm`, it applies that generated
request through the existing journaled lifecycle path.

The config mode can bootstrap the signed TUI and server binaries on a clean
host. It does not add online release discovery, SSH transport, remote host
configuration, backups, updates, public integration, or deployment
verification.

## Verification

- `bash -n scripts/install-linux.sh` — passed.
- `bash scripts/install-linux.sh --help` — exited 0 and listed config mode.
- `python3 tests/test_read_linux_install_config.py` — 4 parser tests passed,
  covering field framing, empty loopback fields, duplicate/unknown keys, and
  the LAN reserved port.
- `python3 -m py_compile scripts/read-linux-install-config.py tests/test_read_linux_install_config.py` — passed.
- `git diff --check` passed for the edited tracked docs/workflow; a separate
  whitespace scan passed for both new Python files and the shell script.
- CI now runs the Bash syntax/help checks and parser tests on `ubuntu-latest`;
  that workflow change has not yet run remotely.

## Limits

The current host is macOS. The root-only configuration-to-apply flow was not
run against Ubuntu, a signed release package, systemd, or Docker Compose.
`shellcheck` is unavailable in this environment. The parser tests validate
configuration content and framing, not the full privileged installer path.

## Fresh local verification — 22 October 2026

- `cargo fmt --all -- --check` and `cargo clippy --locked --all-targets -- -D warnings` passed for `modules/server`.
- `cargo test --locked` passed 349 Rust tests across server, auth, HTTP, installer, TLS, and review suites. The first sandboxed run could not bind its loopback health test; that exact test passed outside the sandbox, followed by a green full suite.
- `go test ./...` passed for `modules/server/tui` after the pinned modules were fetched from `go.sum`.
- The Python install-config parser's 4 tests, Bash syntax check, and relevant nested-repository `git diff --check` runs passed independently.

These checks are source/test evidence only. The Linux installer has not been
run on Ubuntu, and the full TUI still lacks backup, update, and remote SSH
administration workflows.
