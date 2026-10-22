# Scheduled database backup slice — 2026-10-22

## Implementation

Native systemd install plans now own a disabled oneshot service and daily
timer. On a completed native install, the server TUI can run the root-only
`backup enable-schedule` command. The first setup prompts twice with terminal
echo disabled, encrypts the passphrase through `systemd-creds` over stdin, and
stores only the encrypted credential in the root-private installer-owned
configuration directory. Later enable attempts reuse the journaled credential
and can retry enabling or starting the timer without requesting the phrase
again. The operation lock, install record, resource journal, systemd version,
and credential file boundary are checked before the timer is enabled. The TUI
can stop and disable the timer without deleting its credential or archives; an
already-running backup is allowed to finish.

The backup unit uses the installed service account, a bounded six-hour start
time, systemd's per-service credential directory, and writable access only to
the SQLite data and configured backup directories. It does not start the main
server when that service is inactive. Archives are named without overwriting
an existing file and remain passphrase-encrypted. The TUI explains that the
phrase must be retained separately. Scheduled archive retention is manual:
the TUI asks how many newest archives to keep and requires the operator to
type `prune backups`; it deletes only private regular files matching the
scheduled archive name format. The timer does not prune automatically. Compose
scheduling uses a disabled host systemd service and timer included in the
reviewed install plan. Restore cutover remains unavailable, so the combined
backup-and-restore lifecycle capability remains false.

## Manual retention source delta — 2026-10-22

The TUI backup page now offers `P` to prune scheduled archives. The operator
chooses how many newest files to retain (default 30) and types `prune backups`
before deletion. The Rust backend recognizes only
`hs-server-<timestamp>-<uuid>.hsb`, checks the private-file boundary for every
candidate before deleting anything, sorts by timestamp, and syncs the directory
after removal. Manual archives and unrelated directory entries are untouched.
The daily timer still does not prune automatically.

Fresh source checks for this slice:

- `cargo fmt --all -- --check` — passed.
- `cargo check --locked --offline` — passed on `aarch64-apple-darwin`; the
  Unix retention implementation compiled, but no Linux host was used.
- `cargo clippy --locked --offline -- -D warnings` — passed.
- `cargo run --locked --offline -- backup prune --help` — passed and showed the
  new `--directory` and `--keep` options.
- `GOCACHE=/private/tmp/helicoid-tui-go-cache-20261022 go build ./...` — passed
  for the TUI.
- `git diff --check` — passed in the server subrepository and integration
  workspace.
- Tests were not run. No scheduled archive was deleted on a live host, and
  systemd scheduling, automatic retention, Compose retention, and restore
  cutover remain unverified or unavailable.

## Encrypted archive inventory source delta — 2026-10-22

The Rust server now exposes `backup list --directory DIR`, and the TUI Backup
screen uses `l` to list the configured backup directory. The inventory reports
escaped `.hsb` filenames, encrypted byte sizes, and modification times as Unix
seconds. It checks the directory and each matching archive with the existing
owner-private file boundary, rejects symlinks and non-regular archive entries,
and caps the result at 10,000 entries. It reads metadata only; it does not
decrypt or validate archive contents.

Fresh source checks for this delta:

- `cargo fmt --all -- --check` — passed.
- `cargo check --locked --offline` — passed on `aarch64-apple-darwin`.
- `cargo run --locked --offline -- backup list --help` — passed and showed the
  new `--directory` option.
- `GOCACHE=/private/tmp/helicoid-tui-go-cache-20261022 go build -o /private/tmp/helicoid-server-list-build ./...` — passed for the TUI.
- `git diff --check` — passed in the server subrepository.
- Tests were not run. No live archive directory was listed, and restore
  cutover remains unavailable.

Systemd encrypted unit credentials require systemd 250 or newer. `systemd-creds`
uses keys held locally by systemd and may bind encrypted credentials to the
host TPM when available; see the [systemd credential documentation](https://www.freedesktop.org/software/systemd/man/latest/systemd.exec.html)
and [`systemd-creds`](https://www.freedesktop.org/software/systemd/man/latest/systemd-creds.html).

## Build evidence

- `cargo fmt --all -- --check` — passed.
- `cargo check --locked` — passed for the workstation's `aarch64-apple-darwin`
  target. Linux-only adapter execution and systemd unit parsing were not
  compiled or exercised on this target.
- `GOCACHE=/private/tmp/hs-helicoid-tui-build-20261022 go build ./...` — passed
  for the server TUI.
- `git diff --check` — passed in the integration workspace and server
  subrepository.
- Tests were not run.

## Limits

This is source and local-build evidence. The workstation has no Linux Rust
target installed, no systemd manager, and no running Docker daemon. The service
and timer have not been installed or started on Ubuntu; the credential has not
been decrypted by systemd on a host; the daily backup has not been observed;
and failure notification, retention, off-host transfer, Compose host-timer
execution, clean-host restore, and two-host cutover remain unverified or
unavailable.

## Compose scheduling source delta — 2026-10-22

Compose plans now include the host `hs-server` backend and disabled,
journal-owned backup service and timer units. Enabling the schedule uses the
same systemd-encrypted credential mechanism as native systemd installs, with
the Compose credential stored under the root-private installer state directory.
At timer execution, the host backend takes the lifecycle lock, checks the
completed install record and ownership journal, locates the unique container
for the recorded Compose project, and verifies its operation/service labels,
running state, and signed image ID. It then uses the fixed local Docker socket
to execute the backup command in that exact container and sends the credential
over stdin. The container reads one bounded credential value; it is not placed
in argv, an environment variable, or ordinary command output. The host service
is root-owned because the local Docker socket grants host-level control.

The Compose plan owns the backup units, the restricted control socket units,
and the backend artifact. The timer stays disabled after install until an
operator enables it through the TUI.
Disabling retains the encrypted credential and archives. Compose manual backup,
verify, and restore-to-new-file continue to run inside the container. Host
restore/cutover and automated retention remain unavailable.

Focused source checks for this delta, completed before adding the TUI control
socket:

- `cargo fmt --all` — passed.
- `cargo test --locked --offline --lib` — passed, 92/92.
- `cargo test --locked --offline --bin hs-server` — passed, 8/8.
- `cargo test --locked --offline --test installer --test installer_review` —
  passed, 70/70.
- `cargo clippy --locked --offline --all-targets -- -D warnings` — passed.
- `GOCACHE=/private/tmp/hs-go-compose-schedule go test ./...` — passed.
- Full `cargo test --locked --offline` — blocked by the sandbox denying a
  loopback bind in `review_findings::health_on_loopback_socket_still_works`.
- Ubuntu timer installation, credential decryption, Docker stdin delivery, and
  an observed scheduled archive remain unverified because this workstation has
  no Linux systemd host or running Docker daemon.

## Compose TUI host-control socket source delta — 2026-10-22

The Compose release file mounts only a dedicated host runtime directory into
the container. The host systemd socket creates its AF_UNIX endpoint there with
mode `0622`; the service verifies the accepted peer's UID and GID are both
10001 before accepting any request. The socket-activated service uses
`StandardInput=socket` and `StandardOutput=socket`, accepts a bounded binary
enable/disable request with no caller-selected paths, and routes it through the
existing installation journal and lifecycle lock. The Rust container CLI
holds the plaintext passphrase in zeroizing memory
and sends it over the socket; systemd-creds stores only the encrypted host
credential. The host Docker socket remains available only to the scheduled
backup service, never to the server container. Manual Compose startup can create
an empty mount directory, while schedule controls become available only after
the lifecycle installer has started the product-owned socket. See the upstream
[systemd socket documentation](https://www.freedesktop.org/software/systemd/man/latest/systemd.socket.html)
for `Accept=yes`, `DirectoryMode=`, and standard-stream socket activation.

Fresh source checks after the socket delta:

- `cargo fmt --all` — passed.
- `CARGO_TARGET_DIR=/private/tmp/hs-server-compose-schedule-target cargo check --locked --offline --bin hs-server` — passed on `aarch64-apple-darwin`; Linux-only peer-credential and socket-activation code was cfg-excluded.
- `GOCACHE=/private/tmp/hs-go-compose-schedule go build -o /private/tmp/helicoid-server-tui .` — passed.
- `docker compose -f release-compose.yaml config --quiet` — passed; YAML and
  Compose interpolation are valid without starting a container.
- `git diff --check` — passed in the server subrepository.
- No tests were run after the socket delta. The workstation has no Linux Rust
  target, systemd manager, or running Docker daemon; unit parsing, peer-credential
  behavior, container socket mounting, and host timer operation remain unverified.
- The updated Compose template must be included in a newly signed Linux release
  package before deployment.
