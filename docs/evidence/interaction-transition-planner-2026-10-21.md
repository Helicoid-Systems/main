# Interaction transition planner source checkpoint — 2026-10-21

## Implemented

modules/core/src/interaction.rs now exposes a pure per-computer transition
planner for the five interaction kinds and typed machine actions. Its conflict
table covers all 30 interaction-to-interaction cells from
docs/spec/contracts.md. The plan carries the expected interaction revision,
typed refusal, and exact impact details, including interaction owners,
lifecycle state, and fixed share recipients. Inputs are bounded to 20 claims
and 64 recipients per share, and a mixed-computer snapshot is refused.

Replacement plans require explicit consequence review. Foreign claims require
scoped IT stop authority and current permission for the new operation. A claim
already ending produces TeardownPending until the old teardown is reconciled.
Pending release on the unique controller, stale revisions, mismatched
interaction identities, changed operation fingerprints, and another pending
machine action fail closed. A duplicate machine action returns its stored state
only for the same account, manager, typed action, and operation fingerprint.

The server source now depends on hs-core and declares schema version 15. Fresh
stores and the explicit v14 migration create per-computer interaction
revisions and durable claims. `Service::reserve_interaction_transition`
rechecks the current approved operator session and the operation-specific
enrolled-computer assignment in the same transaction: observation requires
`observe`, while the currently supported control/share/attention operations
require `operate`. A positive observation test now uses an observe-only scoped
assignment, and observe-only authority cannot reserve control. Only `Proceed`
inserts a `Requested` claim and advances the revision; exact active retries
return the existing claim, and review-required plans have no side effects.
Foreign-owner stopping remains refused because the current access model has no
scoped IT-stop capability.

The operation fingerprint is an equality input for idempotency only. It is not
authority or a signed grant format. The server derives it from a versioned,
length-prefixed encoding of the typed operation and target computer; callers
cannot supply the digest. This local replay encoding does not freeze the
network request, signed-grant, or manager ABI formats required by E03.

## Verification

- After correcting operation-specific capability selection, fresh
  `cargo check --offline -p hs-server` passed, and
  `cargo test --offline --lib interactions::tests` passed all 5 reservation
  tests, including observe-only observation and observe-only control denial.
- The focused v14-to-v15 migration test passed at the prior source checkpoint.
- The full server run passed 305 tests across the completed test binaries. One
  additional test, `health_on_loopback_socket_still_works`, could not bind its
  loopback listener because this sandbox returned `Operation not permitted`;
  Cargo stopped before running later test binaries.
- `cargo clippy --offline --all-targets -- -D warnings` passed after addressing
  four existing warnings in the touched server crate.
- The endpoint file-delivery integration fixture now binds its request to the
  signed OS-user identity and exact active endpoint session required by the
  server. The focused endpoint enrollment/delivery test passed before the full
  suite run.
- Prior core-only evidence remains: 42 unit tests, 44 integration tests, and
  core clippy passed before server integration.
- After making the server derive the replay fingerprint, `cargo check --locked
  --offline --lib` passed. `cargo check --locked --offline --tests` also passed;
  it compiled the tests but did not execute them.

## Acceptance boundary

This closes a shared-core decision slice and verifies the server-side durable
reservation source slice and migration. The reservation method is not mounted
on an HTTP route. There is no reviewed-plan confirmation, interaction lifecycle
acknowledgment/reconciliation, endpoint admission, signed authority, capture,
input, machine-action execution, concurrency race harness, or installed
endpoint workflow. E03/B03 and V41 remain incomplete; this checkpoint is not
runtime authorization or product acceptance.
