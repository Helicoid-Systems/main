# Restore recovery quarantine release — 2026-10-22

## Implementation

Restoring an encrypted database rotates its deployment and grant-signing
identities, resets bootstrap, and quarantines restored accounts, Manager
approvals, endpoints, and active interactions. MFA confirmation no longer
clears the deployment-wide recovery gate automatically. The local-only
recovery status and release routes require a fresh Manager-bound security
administrator session. Release binds to the deployment identity and reviewed
quarantine counts, requires explicit missing-history and retained-authority
acknowledgments, and records the exact deployment in a confirmation phrase.
Release clears only the deployment-wide gate; restored accounts remain disabled
and quarantined, old Manager approvals stay revoked, and endpoints remain
quarantined until fresh enrollment.

Endpoint token authentication and each final endpoint-session transaction
check the deployment-wide recovery gate. This prevents an already admitted
endpoint session from continuing file delivery, observation, or other endpoint
work while recovery is pending. The Linux TUI exposes `recovery status` and
`recovery release-quarantine` through the literal-loopback API and displays the
reviewed counts before requesting the release acknowledgments. Its interactive
Backup and Restore page now exposes `i` for status and `Q` for the explicit
release prompts; `q` remains the quit key.

The deployment Ed25519 grant signer remains deployment-local in the private
SQLite store, supporting offline and LAN deployments. Encrypted database
backups contain it; restore rotates it before publishing the restored store.
The host administrator and server process remain trusted with signing
authority. This slice does not implement HSM/KMS custody, old-host fencing, or
peer trust cutover.

## Verification

- `cargo fmt --manifest-path modules/server/Cargo.toml -- --check` — passed.
- `cargo check --locked --offline --manifest-path modules/server/Cargo.toml` — passed.
- `gofmt -d *.go` — no formatting differences.
- `GOCACHE=/private/tmp/helicoid-go-build-cache GOPROXY=off go build ./...` in
  `modules/server/tui` — passed.
- `git diff --check` and `git -C modules/server diff --check` — passed.
- Tests were not run.

## Limits

These checks establish formatting and local Rust/Go compilation only. They do
not establish recovery behavior on an installed server, actual encrypted
restore, old-host shutdown/isolation, peer trust adoption, offline peer
reconciliation, public routing, or Qt Manager behavior. The Qt 6.10 SDK is not
available in this environment, so the Manager changes have not been compiled
here. Two-host cutover remains unavailable and must not be reported as
completed.
