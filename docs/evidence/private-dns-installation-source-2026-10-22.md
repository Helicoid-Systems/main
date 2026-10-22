# Private DNS installation source implementation — 2026-10-22

## Implementation

The Linux lifecycle v2 installer now accepts `private_dns` for systemd and
Compose. It requires a private host address, a valid Manager DNS hostname, an
unprivileged HTTPS port, and a matching TLS certificate SAN. The reviewed
resource plan binds only to the configured private address and the digest
continues to bind the exact preflight and certificate observation.

Installer capability discovery advertises private DNS on Linux. The Gum and
plain-terminal installer flows, configuration-driven install parser, TUI plan
command, Qt Manager setup draft, and Manager profile exporter carry the scope
end to end. Plan output gives concrete A/AAAA, TCP allowlist, proxy, hostname,
and certificate instructions. DNS records, firewall and proxy changes, client
CA trust, and certificate renewal remain operator-managed.

The TUI's existing endpoint diagnostic now handles private-DNS deployments. It
resolves the configured hostname, probes only private addresses, and verifies
TLS for that hostname without sending credentials. This diagnostic behavior
was not exercised against a live private DNS deployment.

## Verification

- Go TUI compiled successfully with `go build` on macOS arm64; `gofmt -d`
  reported no changes.
- `cargo fmt --check` passed for the server crate.
- Python AST parsing passed for the Linux configuration parser and Manager
  profile exporter.
- `bash -n` passed for `install-linux.sh`.
- `git diff --check` passed for the server and Qt Manager repositories.
- No test suites were run.
- Rust type-check could not run offline because the local Cargo registry lacks
  the locked `tough` package. The environment did not fetch dependencies.
- The Qt Manager change was not recompiled in this slice; its temporary Qt
  toolchain had been removed during the previously authorized cache cleanup.

These checks establish source formatting and bounded compilation only. They
do not prove installed Linux systemd/Compose behavior, private DNS propagation,
client trust, proxy routing, or live TLS connectivity.
