# Linux installer TUI implementation evidence — 2026-10-20

Source status update (2026-10-21): authenticated v2 file-request routes are
now available on the network TLS router; general file delivery jobs remain
disabled on that router. The Linux TUI still uses its loopback local-server
API. Earlier statements below that describe file requests as loopback-only are
historical. This is source-only and does not establish runtime or E06/S06
acceptance.

This checkpoint connects the Bubble Tea Installation screen to the existing
Rust lifecycle-v2 backend. It adds a root-only guided install that runs local
preflight, verifies the signed Linux package, creates and displays the
configuration-bound plan, requires the exact phrase `install helicoid server`,
and applies that reviewed request. Data and backup directories can be selected
within the existing Helicoid-owned storage roots. A 0600 request is retained
under `/var/tmp` when apply needs attention, and the TUI can resume that request
or retry an already-confirmed removal. The Installation screen displays the
deployment configuration and journal phase states.

## Verification

- `go test ./...` — passed on the macOS host with dependencies in a temporary
  module cache.
- `go build -o /private/tmp/helicoid-server-tui .` — passed on the macOS host.
- `GOOS=linux GOARCH=amd64 go build -o /private/tmp/helicoid-server-tui-linux .`
  — passed; includes the Linux terminal-input implementation.
- `GOOS=linux GOARCH=amd64 go test -c -o /private/tmp/helicoid-server-tui-linux.test .`
  — passed; Linux test binary compiled, not executed on this host.
- `/private/tmp/helicoid-server-tui help` — passed and listed the installer
  commands.

## Boundary

No install, service restart, removal, or data operation was run. The local
administration APIs still use loopback. A separate systemd LAN/TLS install
path is now source implemented as recorded below, but has no host runtime
acceptance yet. Docker Compose, encrypted backup and restore, online update,
and installed-host acceptance remain unsupported. The guided flow also requires a
root-owned `hs-server` lifecycle executable to be present before it starts; it
does not bootstrap that executable. A successful source build or plan review
does not establish that a Linux deployment is healthy or verified.

## Current source changes

The TUI now has a read-only Services and storage view that reports the saved
deployment configuration, installed-service state, readiness, lifecycle
journal phases, and supported backend actions. The view does not claim service
repair or storage recovery. `doctor --json` now preflights the saved custom
storage paths instead of always checking defaults. The Files section can list
owner-scoped jobs and review/cancel an uploading or queued job only when
`/health` reports `features.file_jobs=true`; both actions check that gate before
authentication and safely quote terminal labels. The list shows the server's
delivery expiry in UTC. Cancellation requires the exact job ID and remains
subject to the server's delivery-lease check. The current foundation reports
the feature disabled. The Files section now also exposes bounded request
administration over the loopback development API: list/create drafts, publish
or close a request using its current revision, and review scoped submission
metadata. Draft creation selects enrolled computers from the account's visible
inventory, applies server-aligned limits, and saves only after exact
confirmation. Publication and closure show the request and require its exact
ID. Submission review reports metadata and hashes. A separate download action
requires a received submission and file, confirms its ID, streams bounded
chunks into a private temporary file, verifies size and SHA-256, and publishes
the file without overwriting an existing destination. Every action checks
`features.file_jobs` before authentication. File-job quota and request-content
retention controls remain unavailable. The backup screen now offers explicit
manual pruning of old scheduled database archives; the timer does not prune
automatically. Manual logical content removal for a received request
submission is now available through the loopback development API and TUI.

## Authorized submission content removal source delta

Schema version 14 records whether request content is available and attributes
explicit deletion by time and manager account ID. Migration backfills failed,
expired, and superseded submissions as unavailable, and gives previously
deleted received submissions a best-effort time with unknown actor. TUI
cleanup requires the request owner, current operate scope for the submitting
computer, a received submission, and exact submission-ID confirmation. It
deletes chunk rows, releases the bytes from quota accounting, and keeps audit
metadata. The operation is idempotent and is logical deletion; SQLite free
pages and prior database backups may still contain the bytes. The route remains
loopback-development-only, while the network TLS route returns `501`.

`cargo check --locked --offline`, `cargo fmt`, and `gofmt` have passed for the
current source slices. The Qt Manager change is source-only and has not been
compiled because Qt 6.10 is unavailable here. No live host mutation, runtime
exercise, or acceptance test was run.

`gofmt` completed for the edited Go files. A fresh `go build` could not reach
compilation: the sandbox denied writes to the default Go build cache, and a
retry with writable `/private/tmp` build and module caches could not resolve
`proxy.golang.org` because DNS/network access is unavailable. No downloaded
module artifacts were present in that cache. The earlier build results above
predate these source changes; the current file-request workflows, verified
downloads, and job expiry display delta remain uncompiled. No tests or host
operations were run for this delta.

## 2026-10-21 source update

The v2 file-request and submission routes are now mounted on the network TLS
router independently of the disabled general file-job routes. The Linux TUI
still uses its loopback local-server API; `features.file_jobs=false` applies
only to general delivery jobs. The current Go modules are still unavailable in
the local cache, so these changes have not been compiled. No tests, network
runtime exercise, or host operation was run.

## 2026-10-21 Compose health status source delta

The Compose candidate now uses `hs-server healthcheck` for its container
healthcheck. The CLI probe is limited to a literal-loopback socket, has
bounded connect/read/write timeouts and response bytes, rejects unsupported
transfer encoding and inconsistent content length, and requires `/health` to
report completed bootstrap with authorization accepting work. It does not
verify external DNS, client certificate trust, firewall reachability, or
endpoint enrollment. The source built with `cargo build --locked --offline`,
`cargo fmt --all -- --check` passed, and `docker compose -f
deploy/compose/compose.yaml config --quiet` passed. The CLI rejects a
nonloopback health address. No server/container runtime check, image build,
test suite, or host operation was run. Compose installation remains a source
candidate outside the Rust lifecycle backend.

## 2026-10-21 signed package TUI artifact source delta

The Linux canonical systemd install plan now includes the signed
`artifacts/helicoid-server` TUI and installs it beside `hs-server` at
`/opt/helicoid/bin/helicoid-server`, so later server administration does not
need a Go toolchain on the host. The package-target contract and installer
fixture inputs were updated to require the artifact. A pinned-toolchain
artifact stager and systemd unit template were added. The Cargo formatting
check, offline build, and `git diff --check` passed. `bash -n` passed for
`scripts/build-linux-artifact-dir.sh`. ShellCheck is unavailable. The Linux
artifact build, test suite, and clean-host install were not run. The shell
entry point still requires preinstalled root-owned helper binaries; it does
not bootstrap those helpers.

## 2026-10-21 verified package artifact copy

The native installer now rechecks each signed artifact while streaming it into
a private temporary file, rejects symlink/non-regular sources and size or
digest changes, then atomically renames the verified file into place. This
avoids truncating a running executable during an upgrade and closes the
verify-then-reopen gap for package artifacts. `cargo fmt --all -- --check`,
`cargo build --locked --offline`, and `git diff --check` passed on the current
`aarch64-apple-darwin` host. Only that Rust target is installed, so the
Linux-only adapter change was not compiled for Linux. The Linux artifact build,
installer tests, and host install were not run. ShellCheck is unavailable.

## 2026-10-21 signed Linux package assembly source delta

Added `modules/server/scripts/package-linux-release.sh` to assemble a complete
package from staged Linux artifacts. It requires the server, TUI, and systemd
unit artifacts; rejects symlinked or non-regular inputs and non-portable paths;
checks both executables' copied ELF headers for Linux x86_64; hashes the copied
artifact bytes; derives the schema and installer floors from the source
constants; signs the exact manifest with an externally supplied
Ed25519 PEM key; and verifies the generated signature before publishing the
package directory. It prints the raw public key separately for trusted release
distribution and does not include the trust anchor in the package. `bash -n`,
the script help path, embedded Python syntax
compilation, and `git diff --check` passed. The packager was not run against a
real key or built artifacts, and no signed package was generated.

## 2026-10-21 systemd service boundary source delta

Both the packaged default unit and the v2-generated unit now run with no
ambient capabilities, `NoNewPrivileges`, a read-only system view, isolated
temporary files and devices, kernel and namespace protections, and a single
configuration-bound writable data directory. Rust formatting and the
`aarch64-apple-darwin` offline build passed; the Linux target is not installed,
and the unit was not parsed by systemd or exercised on a host. No tests or host
operations were run. Clippy completed with four current warnings in
`src/crypto.rs` and `src/file_requests.rs`; the installer changes emitted none.

## 2026-10-21 clean-host bootstrap source delta

The interactive Linux installer can now bootstrap its own helper binaries when
no trusted `helicoid-server` is installed. A small Python standard-library
helper reads the package manifest and signature through no-follow file
descriptors, checks the Ed25519 signature with OpenSSL, enforces the fixed
three-artifact manifest shape, hashes while copying into private staging, and
checks both executable ELF headers for Linux x86_64. The shell then installs
only the TUI and Rust backend beneath the protected `/opt/helicoid/bin`
directory; the existing Rust installer still performs the full package,
preflight, and plan-digest review before service changes. The flow requires
root, systemd, Python 3, OpenSSL, the extracted signed package, and a public
key obtained from a separate trusted channel. Non-interactive plan/apply still
requires the helper binaries to be installed already. No tests, shell syntax
check, signed artifact build, package verification run, or host operation was
performed for this source change.

## 2026-10-21 Qt Manager v2 route integration source delta

The Qt Manager now loads authorized inventory, account records, Manager
records, and access assignments from authenticated v2 routes, and uses v2
logout. The TLS network router exposes these bounded reads. Manager approval
and revocation also have v2 handlers that require fresh MFA and an approved
key-bearing Manager session; revocation requires confirmation of the exact
Manager ID and revokes its active sessions in the same transaction. The v2
contract now documents those reads and writes. No tests, build, TLS connection,
or runtime Manager operation was performed for this source change.

## 2026-10-21 observe-scope viewer gate source delta

The authenticated v2 API now returns only the current account's `observe`
computer scope, bounded to the inventory page limit. The Qt Manager refreshes
that scope per deployment, generation-fences responses against the active
session, expires cached scope after a monotonic 30 seconds, and closes lifecycle viewer
windows when scope is refreshed, expires, becomes unavailable, or is
invalidated. Opening a viewer requires the selected computer to be in the current scope (or an all-computers
assignment). This remains a client-side gate and does not authorize capture;
endpoint admission and pixel transport remain unavailable. No tests, build,
TLS connection, or runtime Manager operation was performed for this source
change.

## 2026-10-21 Linux TUI v2 administration routes source delta

The Linux TUI's account, access-assignment, computer, and manager reads now use
the v2 APIs. Its loopback logout also uses v2. Manager approval and revocation
now call the v2 endpoints; revocation confirms the exact manager ID, and both
operations inherit the server's fresh-MFA and approved-device checks. The TUI
continues to use the loopback listener for its local-server workflow. No tests,
build, or runtime administration session was performed for this source change.

## 2026-10-21 read-only TUI network diagnostics source delta

The Network and certificates screen now reads local interface addresses and,
on Linux, nameserver IPs from `/etc/resolv.conf`. It caps collection at 128
interfaces, 512 addresses, a 64 KiB resolver file, 16 nameservers, and 16
warnings, then caps display at 32 interfaces and 64 addresses; it normalizes IP
values and quotes interface names before terminal display. In LAN mode it shows
the public certificate-chain SHA-256 from the last local preflight only when
the network configuration still matches; it does not reread the certificate.
It performs no DNS, TLS, firewall, remote-reachability, or public-edge probe and
changes no host state. `gofmt` passed. The TUI package was not built:
its pinned Charm dependencies are not in the writable module cache, and the
earlier build attempt was blocked when Go tried to populate the read-only
default module cache. No tests or runtime TUI interaction were run.

## 2026-10-21 Linux systemd LAN/TLS source delta

The v2 Linux systemd contract now accepts a private local IPv4/IPv6 bind
address and an unprivileged TLS authority port, defaulting to TCP 8920. An
optional `client_hostname` binds private DNS guidance and the exported Manager
profile to the hostname clients will use; it does not alter the listener bind.
installer preflight checks that the address is local, that the port is
available, and that the root-owned certificate chain and matching private key
at `/etc/ssl/helicoid/` pass held-file TLS validation. It also checks that the
leaf certificate SAN covers the configured Manager hostname, or the bind IP
when no hostname is configured. This does not verify the issuer or client
trust. The exact
public certificate-chain digest is included in the reviewed plan and rechecked
before apply and resume. The generated unit keeps the local HTTP listener on
127.0.0.1:8787 and exposes only the v2 router through the LAN TLS listener. It
uses systemd per-service credentials for the certificate/key; key contents do
not appear in argv. Neither firewall, DNS, proxy, nor client trust settings
are changed by the installer. The Gum shell flow and Go TUI default to LAN and
retain explicit loopback development configuration.

`cargo check --offline -p hs-server`, `bash -n scripts/install-linux.sh`,
`gofmt`, and `git diff --check` passed. A current Go build could not begin
because the pinned Charm modules are absent from the offline module cache and
the sandbox cannot write the default cache; no tests were run. No systemd unit
parse, LAN listener, certificate load under systemd, client trust, firewall,
or live host install was exercised. This source delta does not establish LAN
deployment acceptance.

## 2026-10-21 Qt Manager setup draft network fields

The Manager's local Server setup draft now selects LAN (default) or explicit
loopback development, collects the private bind address and LAN HTTPS port, and
validates RFC1918 IPv4 or unique-local IPv6 address syntax plus an optional DNS
hostname before saving. The
LAN hint names the expected certificate chain/key paths and root-only file
modes, and explains that the certificate SAN must match the client DNS name or
IP and the issuer must be trusted by clients. This remains a local draft: it
does not contact the Linux host, inspect interfaces, or validate certificate
files. The Manager setup source has not
been compiled because Qt 6.10 is unavailable in this workspace; no interactive
UI path or remote install was exercised.

## 2026-10-21 Linux installer connection-profile handoff source delta

After a successful interactive run invoked through `sudo`, the shell installer
can export a schema-v1 Manager profile matching the Qt import format. The
profile endpoint comes from the durable installed configuration, the file is
created as the invoking non-root operator with mode `0600`, and existing paths
are not overwritten. It uses the configured Manager DNS hostname when present,
otherwise the private bind address. It carries no credentials, TLS key, or deployment trust
pin; Manager still requires identity verification through the trusted
organization channel. The installer also offers to launch the installed
`helicoid-server` TUI as that operator. The profile generation and TUI launch
have not been exercised against a signed release artifact or installed host.

## 2026-10-21 Linux TLS subject-name preflight source delta

LAN plan review, apply, and resume now validate that the leaf certificate
covers the configured Manager DNS hostname, or the private bind IP when no
hostname is configured. The name check uses the held-read certificate bytes
also used to build the TLS server configuration and compute the reviewed
certificate-chain digest. It does not validate issuer trust, chain signatures,
certificate time, DNS resolution, or remote client trust. Shell and TUI guidance
now states that SAN matching is checked during preflight and that CA trust is
an operator prerequisite.

`rustfmt --check`, `cargo check --offline -p hs-server`, `bash -n`, `gofmt`, and
`git diff --check` passed. No tests were run. Go compilation, certificate
runtime behavior, systemd credential loading, and a live installation remain
unverified.

The v2 preflight response now carries `tls_subject_name_verified`. Older
backends that omit this additive field are reported as unverified by the TUI.
The reviewed-plan digest binds the result, and plan/apply/resume compare it with
the current certificate validation.

## 2026-10-21 redacted support export source delta

`helicoid-server support export [--output NEW_FILE]` and the TUI Diagnostics
screen's `e` action now write a bounded schema-v1 JSON support bundle. Output
uses allowlisted health and lifecycle enums, capability names, and aggregate
diagnostic counts; arbitrary error strings, paths, identities, addresses,
credentials, logs, audit events, and database contents are excluded. The bundle
is limited to 64 KiB, staged mode `0600`, and published with a no-overwrite
hard link. It is local-only and is not uploaded. The current backend does not
expose an audit-event route or log export, so this slice records that gap rather
than implying those records are included.

`gofmt` completed for the touched Go sources. The Go executable was not built
and no tests were run for this slice; prior workspace dependency resolution is
blocked by unavailable `proxy.golang.org` DNS. This is source evidence only.

## Fresh TUI and backup verification — 2026-10-22

- `GOCACHE=/private/tmp/hel-go-build-cache go test ./...` in `modules/server/tui` passed (`ok helicoid-server`, 0.331s).
- `CARGO_TARGET_DIR=/private/tmp/hel-server-target cargo test --locked --offline --lib backup::tests::restore_fences_prior_authority_and_never_overwrites_destination` in `modules/server` passed (1/1).
- The Go tests cover the current TUI package. The Rust test verifies restore fencing and no-overwrite behavior on this Unix host.
- The signed Compose package mounts the configured backup directory into the container, and the TUI exposes manual backup, verify, and restore-to-new-file commands there. This source path has not been exercised inside a built Compose image. Scheduled backups remain systemd-only; Compose update and deployment cutover are unavailable.
- No Linux host mutation, Compose image build, installed backup schedule, or restore cutover was run.

## 2026-10-22 bounded event diagnostics follow-up

The earlier schema-v1 support-export and missing-log statements above describe
the state at that time. The TUI now exports schema-v2 bundles with bounded,
allowlisted service-event metadata and offers `logs recent` for both systemd
and Compose source paths. See [the follow-up evidence](linux-tui-bounded-service-events-2026-10-22.md)
for implementation boundaries and fresh compile results. Audit export and
installed Linux runtime proof remain outstanding.
