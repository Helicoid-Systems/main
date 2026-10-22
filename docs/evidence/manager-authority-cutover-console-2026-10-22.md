# Manager authority cutover controls

## Implemented source

The shared Qt Manager Server ribbon now opens the selected deployment's
authority cutover status and supports these v2 operations:

- Read `/v2/authority/cutover` after rechecking the saved deployment ID,
  grant-signing key, and authority generation.
- Begin drain with a fresh-MFA session and an exact deployment phrase.
- Cancel only a still-draining predecessor with a fresh-MFA session and exact
  cutover phrase.
- Approve a restored replacement with fresh MFA, exact predecessor and
  replacement IDs, one of the server's fixed fencing methods, and a bounded
  evidence reference.
- Read the restore-quarantine summary and release its deployment-level gate
  after fresh MFA, cutover activation, a count snapshot, both required recovery
  acknowledgments, and an exact deployment phrase. Historical accounts and
  endpoints remain individually quarantined, and revoked Manager devices stay
  revoked.

The UI explicitly identifies the approval as an operator attestation. The
server does not verify the old host's state or endpoint trust adoption. The
Manager stores a locally verified generation checkpoint and blocks protected
identity flows when the live checkpoint differs. It still does not report
per-peer reconciliation.

## Verification

- Inspected the route definitions and request DTOs in `modules/server`.
- Confirmed the Manager action performs a saved-identity check before opening
  the workflow and disables mutations when status is invalid or unavailable.
- The isolated Manager registry and native WebRTC ABI tests pass (2/2).
- The full Manager target could not configure because Qt 6.10 is not installed
  in this environment. No Qt source build or interactive UI run is claimed.

## Acceptance still required

Build and exercise the shared Manager on macOS and Windows, then verify the
workflow against a server with fresh MFA, live-work drain refusal, predecessor
cancellation, independent reviewer approval, restored-server quarantine, and
the operator-attestation limitation. Two-host fencing and per-peer trust
adoption remain separate V47 requirements.
