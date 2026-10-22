# Endpoint recovery re-enrollment controls

The Windows Computer and macOS Computer source now expose an explicit
forget-and-re-enroll path for recovery when an endpoint identity must be
replaced.

- Both flows require an explicit destructive confirmation, stop endpoint
  polling, remove local enrollment metadata and the endpoint signing key, and
  preserve files already received.
- Windows deletes the protected device key before enrollment metadata. If key
  deletion fails, metadata remains so the reset can be retried safely.
- macOS deletes the deployment-scoped Keychain item, clears in-memory endpoint
  session/admission state, waits for its file polling task to stop, and clears
  deployment lookup hints. Session generation checks prevent an overlapping
  handshake from repopulating the removed cache.
- A fresh invitation creates a new key and computer identity. The old server
  record and queued jobs remain scoped to that identity; local deletion does
  not quarantine the record, fence an old host, or prove peer trust adoption.
- A live generation mismatch clears the affected deployment's local session
  and observation state without clearing state for other connected servers.
- macOS replay and active-admission indexes use deployment ID plus grant ID,
  so duplicate grant labels from separate servers cannot overwrite one
  another's local observation guard.

Endpoint enrollment schema version 4 also stores the verified deployment
authority generation with its grant-signing key. Mac and Windows protected
session paths re-fetch `/v2/deployment/authority` and refuse file or observation
work when the live deployment ID, key, or generation differs. A mismatched
endpoint stays unavailable pending recovery review and local re-enrollment.

Verification: the Core-only `EndpointEnrollmentClientTests` run passed all 8
tests after the checkpoint change, including checkpoint persistence, changed-
generation refusal, forget-and-fresh-key enrollment, file receipt, and
observation admission. The latest isolated run also covers the deployment-
scoped cleanup refinement. It used the app's macOS 13 exclusions and only the
endpoint test file. The normal package also builds `ComputerPermissionTests.swift`,
which currently fails with main-actor-isolation errors. The full macOS package
and UI interaction remain unverified. Windows WinUI build and interactive
confirmation remain unverified because this Windows App SDK project cannot
build on the current host and `dotnet` is unavailable here.
