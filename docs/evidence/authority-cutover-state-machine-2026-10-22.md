# Authority cutover state machine: 22 October 2026

## Implemented

- Storage schema 19 persists `authority_state`, `authority_generation`, the
  predecessor authority ID, and a durable cutover review record.
- `helicoid-server cutover drain` requires zero active interactions,
  unconsumed observation grants, and uploading/queued/in-progress file jobs.
  It revokes endpoint sessions, consumes pending authentication challenges,
  and stops ordinary authorization. Recovery and cutover review remain
  available. The predecessor can cancel only while its own database is still
  Draining.
- Encrypted database restore rotates the deployment and grant-signing
  identities, advances the generation, records the predecessor/replacement
  pair, and leaves the replacement Awaiting Cutover.
- A fresh-MFA security administrator records a fencing method and evidence
  reference against the exact predecessor and replacement identities. The
  approval activates the replacement authority. Restore quarantine release
  refuses until this approval commits.
- The loopback TUI exposes `cutover status`, `drain`, `approve`, and `cancel`.
  The cutover routes are also present on the v2 TLS network router.

## Verification

- `cargo check --manifest-path modules/server/Cargo.toml --all-targets` passed.
- `cargo test --manifest-path modules/server/Cargo.toml --lib` passed: 95
  tests, including the drain, self-approval denial, independent reviewer,
  live-work drain refusal, endpoint-session denial, restore-generation, and
  quarantine-release ordering checks.
- `cargo test --manifest-path modules/server/Cargo.toml --test http_api`
  passed: 18 tests, including network-router cutover route exposure.
- `cargo test --manifest-path modules/server/Cargo.toml --test auth_v2`
  passed: 62 tests.
- `GOCACHE=/private/tmp/hel-go-cache go test -mod=readonly ./...` in
  `modules/server/tui` passed.
- Full `--all-targets` execution is environment-limited: the loopback health
  test and eight TLS listener tests failed at bind with `Operation not
  permitted`. The unit, auth, HTTP, installer, package-target, installer
  review, and MFA-boundary targets passed during the serialized run.

## Acceptance boundary

The evidence reference and selected fencing procedure are operator attestations;
the server does not inspect the predecessor host or prove shutdown. The public
authority response now includes the persisted generation. Manager profiles
store the locally verified ID/key/generation and reject rollback or a
same-generation key replacement. Mac and Windows endpoint enrollment records
store the checkpoint and protected session paths fail closed when the live
authority differs. There is still no per-peer adoption result workflow, and
offline peers, parallel restores from the same pre-cutover backup, and old-host
return have not been exercised on two real hosts. This work establishes a
durable server gate and local mismatch checks; it does not pass V47 or complete
authority cutover.
