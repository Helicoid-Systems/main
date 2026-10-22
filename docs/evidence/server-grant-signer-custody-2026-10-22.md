# Server grant signer custody — 2026-10-22

## Implementation

The deployment's Ed25519 seed is generated locally and stored in the server's
owner-private SQLite database. Persisted authority is disabled unless the
database file and its parent boundary pass the existing private-store checks.
The seed is not separately encrypted in the live DB and is not held in an HSM
or KMS; the host administrator and server process are trusted with signing
authority. Generation, restore rotation, and reads use zeroizing
application-owned buffers. Passphrase-encrypted database backups include this
DB-resident key; restore rotates it before publishing the recovered store.

The Ed25519 dependency explicitly enables its zeroize support. The authority
route returns the public key only, and missing or malformed persisted key
material fails closed. Observation-grant issuance and one-use redemption now
use this signer; grant bindings and endpoint-client verification are recorded
in [`observation-grant-issuance-2026-10-22.md`](observation-grant-issuance-2026-10-22.md).

## Verification

- `cargo fmt --check` passed.
- `cargo test --locked --lib` passed: 84 tests, including signer persistence,
  private-store migration, and restore rotation.
- `cargo test --locked --test http_api network_router_publishes_only_the_deployment_grant_verification_key`
  passed: 1 test.
- `cargo clippy --locked --all-targets -- -D warnings` passed.
- `git diff --check` passed in `modules/server`.

The standalone Manager registry tests pass separately. Full Qt Manager build
and installed workflows remain unverified because Qt 6.10 is absent. This
evidence establishes deployment-local signer custody only. It does not
establish HSM/KMS custody, endpoint media admission, or production deployment
security.
