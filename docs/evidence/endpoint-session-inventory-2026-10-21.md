# Endpoint session inventory source checkpoint — 2026-10-21

## Implemented

The v2 `GET /v2/computers` handler now adds an optional `endpoint_session`
summary for enrolled computers inside the caller's current observe assignment.
It reports the latest authenticated endpoint session for the current
enrollment epoch, its last key-proof time, whether its bearer session remains
unrevoked and unexpired, and whether that session reported an eligible signed-in
console user. It does not expose the OS user ID or display label. Operate-only
inventory and v1 inventory omit the summary.

The Qt Manager parses the bounded optional fields and displays the last key
proof, current endpoint-session expiry, and console-user report in the
Computers table and tiles. The UI labels these as session facts; it does not
claim that the endpoint is currently connected, that capture permission is
ready, or that pixels are authorized.

This reuses storage schema v15 and introduces no new database state. The v2 HTTP
contract now documents the additive response field and corrects its storage
schema version to 15.

## Verification

- `cargo check --locked --offline --tests` passed for `hs-server`.
- `cargo test --locked --offline --lib interactions::tests::v2_inventory` passed
  the observe-scope visibility, current-session summary, and old-enrollment
  epoch fencing cases.
- `cargo clippy --locked --offline --all-targets -- -D warnings` passed for
  `hs-server`.
- The Qt Manager has not been compiled: the available host lacks the required
  Qt 6.10 package configuration.

## Acceptance boundary

This exposes authenticated endpoint-session history to observe-authorized
Managers. It is not a live-presence lease and does not implement server-issued
observation admission, endpoint capture, frame transport, expiry/revocation
teardown, or the first authorized preview. `screen_observation` remains false.
