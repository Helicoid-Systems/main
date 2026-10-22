# Endpoint admission and file-job implementation evidence

Date: 2026-10-20

Source status update (2026-10-21): the network TLS router now serves the
authenticated v2 file-request lifecycle. Statements below that say request
routes are loopback-only or return `501` describe the earlier snapshot; see the
source update at the end. The earlier single-server Manager description is
also superseded by the combined-target update at the end. This does not
establish runtime or E06/S06 acceptance.

## Implemented

The server now has explicit endpoint challenge/complete/self routes under
`/v2/endpoints`. Enrollment proof binds an endpoint key to a deployment and
approved enrollment epoch. Challenges enforce bounded lifetime, clock skew,
nonce replay storage, and per-computer/server issue limits. Endpoint session
tokens are opaque, stored as hashes, short-lived, and rechecked against the
current enrollment identity and epoch on each request. Revocation therefore
fences existing endpoint sessions.

The server also has a bounded manager-to-endpoint file-job API under
`/v2/files/jobs` and `/v2/endpoints/jobs`. Jobs have explicit enrolled targets,
independent durable results, SHA-256 verification over 256 KiB chunks, size,
target, active-job, storage, upload-expiry, and recipient-wait limits, and
cancellation only before endpoint lease. A recipient wait expires 24 hours
after upload verification; the server records the deadline in manager and
endpoint job views, expires unfinished targets, and deletes payload chunks from
a periodic maintenance pass. Endpoint revocation fails that endpoint's pending
or in-progress targets while allowing other targets to continue. Payload
chunks are removed once all targets are terminal.

The production TLS router now returns `501` for those routes while
`features.file_jobs` is false. The loopback development router retains the
service routes for contract tests; desktop endpoints cannot use that route.

The Qt Manager Files workspace selects one owning server and local file, then
can choose up to 50 enrolled computers on that server. It preflights each
recipient session in sequence, shows all named recipients in one confirmation,
submits one server-local job, and uploads the file in bounded chunks. It refreshes
job history, displays per-target states, and requests cancellation with exact
job-ID entry. The local file is checked for size/content changes during
preparation and upload. The server remains the authority for operate scope and
job state. The Manager documentation describes the source limits and current
combined-target behavior below.

Both Computer source clients now include an endpoint receiver. Each performs
the endpoint challenge and completion proof, verifies the returned self
identity, and polls only its own bounded endpoint job list every two minutes.
That interval stays under the endpoint challenge rate limit. macOS keeps the
server origin with the Keychain identity; Windows stores the origin and pinned
CA trust in its DPAPI-protected enrollment record. The clients check job
metadata, chunk sizes, declared size, and the full SHA-256 digest; macOS also
checks the server's per-chunk digest header. They write to private per-user
Received Files folders, refuse to replace an existing file, and report success
only after the verified file is published. Both apps expose delivery status
and a folder-opening action. Polling starts after enrollment has been saved.

The D29 source path now includes an optional `local_user_id` in the endpoint
session proof. macOS reports the console user's UID only when it matches the
process UID; Windows reports the process token SID only when the process runs
in the active physical-console session. The server stores that identity and
its signed display label beginning in schema version 9, binds each new file target to the
latest fresh verified endpoint session, and requires an exact match for job
leasing, chunk reads, and result commits. Old sessions and migrated targets
without an identity fail closed. The Manager labels waiting and unavailable recipients. Both clients
recheck the same OS identity during transfer and immediately before local
publication; identity changes leave unfinished work resumable instead of
reporting terminal failure.
When a client detects that it no longer has an eligible console user, it
completes a signed unbound endpoint session to revoke the previous session;
that session cannot call file-job routes. The server and Manager report the
newest session consistently and never fall back to an older local identity.

macOS walks the received-files path without following symlinks, checks each
ancestor for another user's write access, pins the private destination directory
by file descriptor, and publishes with a same-directory exclusive rename. It
rechecks the path's device/inode and permissions before and after publication.
Windows holds its private destination directory and ancestor handles through
transfer and commit, then rechecks the active console session before reporting.

The D29 confirmation now has a signed account label. The Manager fetches a
three-minute-fresh recipient session before showing the account and computer,
then submits that exact session ID with job creation. The server rejects a
changed recipient and returns the signed display label in the target receipt
and job history. The label is for human confirmation; the stable OS user ID
remains the authorization binding. The Manager also pins the reviewed server
origin across preflight, job creation, and every upload chunk so a profile edit
cannot redirect the payload. File jobs remain disabled on the production TLS
router.

## Verification

The following verification was completed for the earlier D29 foundation,
before the signed display-label and manager preflight refinement:

- `cargo test --locked --test http_api`: 17 passed, including endpoint proof,
  replay, hashed token storage, identity/epoch revocation, file upload integrity,
  target isolation, cancellation, and revocation behavior.
- `cargo test --locked --test http_api network_router_exposes_v2_and_health_but_not_legacy_v1`
  passed after adding the production file-route `501` assertion.
- The full Rust target suite passed with the following loopback socket tests
  excluded because this sandbox denies local `listen(127.0.0.1)`:
  `health_on_loopback_socket_still_works`,
  `valid_test_ca_hostname_serves_real_health_json`,
  `foreign_ca_is_refused`, `wrong_hostname_is_refused`,
  `plaintext_request_cannot_access_http_body`,
  `stalled_handshake_is_bounded_and_dropped`,
  `graceful_shutdown_is_bounded`,
  `actual_cli_https_health_and_bounded_sigint`, and
  `actual_cli_public_bind_refuses_with_zero_db`.
- `cargo clippy --locked --all-targets -- -D warnings` passed.
- `cargo fmt --all -- --check` passed; `git diff --check` was clean for the
  repository-tracked diffs. A direct trailing-whitespace scan over the new
  Manager source, README, and this evidence note found no matches.
- `swift test` passed with 238 Core, 18 Manager, and 14 Computer tests. The
  endpoint tests cover enrollment persistence, the fail-closed health feature
  gate, endpoint session requests, a verified file write, and successful
  result reporting.
- `swiftc -frontend -parse` passed for the edited macOS endpoint client,
  Computer model/UI, and endpoint tests.
- Windows source was not compiled: `dotnet` is not installed in this
  environment. No installed Windows or macOS endpoint transfer was exercised.

The current source build evidence is `cargo fmt && cargo check --locked`
(passed with schema version 9 and signed display labels) and
`swift build --scratch-path /private/tmp/hs-swift-build` (passed with the signed
display-label proof). `dotnet build` could not run because `dotnet` is not
installed; no Windows compile is claimed. No test command was run for this D29
change.

The Qt Manager changes are source-only here. The configured Qt 6.10 CMake
package is unavailable in this environment, so `main_window.cpp` has not been
compiled or exercised in the desktop application in this workspace.

## Acceptance boundary

The endpoint receiver is still a protocol candidate, not a complete or
accepted delivery workflow. The 2026-10-21 source update below now mounts the
authenticated v2 job routes on TLS and loopback and advertises
`features.file_jobs=true`; the earlier disabled-route statements in this note
describe the preceding snapshot. This does not establish installed endpoint
delivery, same-user teardown, or S06 acceptance. D29's session, target, and
local-user checks now exist in source, but V48 remains NOT VERIFIED: no Windows build, real-server
transfer, logout/user-switch race, ACL/path race, or installed Windows/macOS
commit was exercised for this change. Existing macOS fixture tests are
scripted HTTP and temporary-directory evidence, not live deployment proof.
File-request draft/publish/close, endpoint collection API, signed-user-bound
submission storage, and scoped manager metadata/content retrieval are source
present in the loopback router. The Qt Manager also has source UI for saving,
listing, publishing and closing requests behind its request-availability check,
combining rows across signed-in servers by default while keeping each mutation
bound to its owning server. Manager submission review/download source is
present, but the app has not been compiled or exercised. Windows and macOS
endpoint request picker/upload source is present; platform builds and live
endpoint verification remain pending. Retention controls and deployment
enablement remain unavailable. SQLite payload chunks are protected by the existing private
data-directory permissions, not application-level encryption at rest. No
signed package, public-network pilot, or release acceptance is claimed.

## Recipient-wait expiry source delta

The current source adds schema version 10 with a persistent delivery deadline.
Uploads retain their 24-hour upload deadline; after hash verification, a job
gets a fresh 24-hour development-candidate deadline for the intended local
user to return. E06 must freeze this budget before file jobs are enabled. Server
maintenance runs every minute in both HTTP and TLS serving modes, expires
unfinished targets, deletes their chunks, and preserves already completed
target results. Manager and endpoint job views expose the deadline, and the Qt
Files table and Linux TUI display it. Existing active v9 jobs receive a
bounded migration deadline.

`cargo fmt --check` and `cargo check --locked --offline` pass for the Rust
schema and service changes. The edited Qt Manager and Go TUI have not been
compiled, and this expiry path has not been runtime exercised. Earlier
integration verification above predates schema version 10 and this expiry
implementation. The production feature gate remains false, so this source does
not enable file jobs.

## File-request backend source delta

Schema version 12 adds owner-scoped request drafts, immutable eligible-computer
bindings, revisioned publish/close transitions, bounded manifests, OS-user-
bound submissions, resumable chunks and manager retrieval by current operate
scope. The endpoint binds a submission to the signed local user ID and label;
another user on the same computer cannot resume, replace or retrieve it.
Replacement requires explicit confirmation, same-user ownership and a request
policy allowing it. The previous content remains intact until the new manifest
passes SHA-256 verification. Request submissions and file jobs share one 2 GiB
payload reservation cap. Closing/expiry rejects new work, expires pending
uploads and removes temporary chunks while preserving Received content. Idle
maintenance runs the same deadline cleanup in both serve modes.

`cargo fmt --check`, `cargo check --locked --offline`, and diff whitespace
checks passed. No tests were added or run and no migration was applied to a
live data store. The Linux TUI now includes source workflows for request
administration, submission review, and verified file retrieval; its broader
network, retention, and cleanup controls remain unavailable. Backup-retention
and deletion controls, cross-client runtime proof, and D14/E06 acceptance
remain outstanding. All request routes remain
`501` on the network-facing TLS router and the file feature stays disabled.
This backend source is not S06 acceptance.

The Manager now includes source UI to save a draft, combine request rows from
signed-in servers by default, publish a selected draft, and close an open
request after typing its exact ID. The list refreshes when opened or filtered;
row actions stay bound to the owning server. It also lists received submissions, lets the operator
choose one received file, and saves it through `QSaveFile` in bounded chunks.
The download rechecks the owning session generation and server origin after the
save dialog, validates safe single-component filenames and chunk metadata,
checks final size and SHA-256, then commits the destination atomically. Each
request action rechecks `/health`; a false `features.file_jobs` is accepted
only for an HTTP loopback development endpoint, whose request routes are
mounted independently of delivery jobs. Other endpoints remain gated.
The Manager configure attempt used
`cmake -S apps/manager -B /private/tmp/helicoid-manager-build` and stopped at
`find_package(Qt6 6.10 REQUIRED)`: no Qt 6.10 package configuration is
installed or discoverable here. The Manager source is therefore not compiled or
runtime exercised. The macOS Computer client now has source UI for refreshing
eligible file requests, reviewing signed-user submission history, selecting
local files, resuming matching partial uploads, and explicitly confirming a
replacement by submission ID. It streams source files in 256 KiB chunks after
no-follow open, bounded size and SHA-256 preparation, rechecks the signed local
UID before each chunk, and waits for the server's hash-verified Received
receipt. The macOS package build passed with
`CLANG_MODULE_CACHE_PATH=/private/tmp/helicoid-clang-module-cache swift build --disable-sandbox --scratch-path /private/tmp/helicoid-swift-build`.
The Windows Computer client now has source UI for listing assigned requests,
showing current signed-user submission history, selecting files through the
native multi-file picker, and confirming an exact prior submission before
replacement. It opens selected files through Windows handles that reject
reparse points and directories, hashes bounded manifests, uploads 256 KiB
chunks, validates chunk receipts, and checks the final server-verified
`received` state. `dotnet` is unavailable in this environment, so Windows
source has not been built or exercised. No tests were run. Earlier statements
that request UI and upload runtime were unavailable through TLS because
`features.file_jobs` was false describe the prior snapshot; retention
administration and S06 acceptance remain incomplete.

## Authorized submission content removal source delta

The server schema is now version 14. It records whether submission content is
available, plus the time and manager account ID for explicit deletion. An owner with current operate scope can delete content for a
received submission only after the request and exact submission ID are bound
and confirmed. Chunk rows are deleted and quota accounting stops counting the
content; submission and file metadata remain. Repeated deletion is idempotent.
Existing deletions receive their `updated_at` as a best-effort deletion time;
their manager account ID remains unknown.
Expiry, failed verification, and replacement mark prior content unavailable.
This is logical deletion: SQLite free pages and pre-existing backups can still
contain old bytes. The DELETE route is mounted only in the literal-loopback
development router; the network TLS router continues to return `501`.

The Linux TUI exposes the action as `files requests delete-submission` and the
Files screen `x` action. The Qt Manager source now also has an explicit removal
button, typed submission-ID confirmation, and a validated receipt path. Its
request helper checks healthy server state and current session before dispatch;
it does not use `features.file_jobs` as a request-workflow gate. `cargo check --locked
--offline` passed after the server changes. The Qt Manager was not
compiled because Qt 6.10 is unavailable here; no runtime or acceptance test was
run. General file-job routes, scheduled retention, physical erasure, and S06
acceptance remain outstanding.


## 2026-10-21 source update: authenticated file-request routes

The server now mounts the owner-scoped v2 request, submission, chunk, and
content-removal routes on the network TLS router as well as the loopback
development router. The `/health` v1 feature shape remains unchanged;
`file_jobs=false` still disables general delivery jobs and no longer gates the
separate request workflow in the Qt Manager, Linux TUI, or endpoint clients.
This is source routing only. No tests or network runtime exercise were run for
this update; Qt and Go builds remain unavailable in this environment, and
retention, storage-boundary, and E06/S06 acceptance remain incomplete.

## 2026-10-21 source update: recipient-bound file-job network routes

The authenticated recipient-bound v2 delivery routes are now mounted on both
the TLS network router and the loopback development router. The health feature
flag reports `file_jobs=true`, so the existing Manager and endpoint clients can
enter their guarded flows. The v1 `/v1/files/jobs` route remains unavailable.
Requests still require a fresh signed endpoint session for the same verified
local user, exact recipient-session confirmation, bounded chunks and payload,
and the existing delivery expiry. A follow-up `cargo check --offline -p hs-server`
passed after this source change, as did `git diff --check`; no tests were run.
Qt and Windows endpoint builds, live
same-user delivery and user-change teardown, cross-server partitioning, quota
and retention acceptance, and V48/S06 runtime evidence remain outstanding.

The Linux TUI build could not fetch its pinned Charm modules because the
sandbox denied writes to the default Go module cache. The Qt 6.10 package is
unavailable in this workspace. No TUI, Qt, installed endpoint, or runtime
acceptance is claimed.

## 2026-10-21 portable Windows client compile

Using the bundled .NET 10.0.401 SDK, `dotnet test
src/Helicoid.Domain.Tests/Helicoid.Domain.Tests.csproj --no-restore -m:1
/nodeReuse:false` compiled `Helicoid.Domain`, `Helicoid.WindowsPlatform`
(including the endpoint file-request upload client), and
`Helicoid.Domain.Tests`. The test host then aborted before running test cases
because it could not bind its local TCP communication socket (`EPERM`). The
initial compile exposed an invalid XML `paramref` in the Windows enrollment
client; the comment now names its saved deployment trust without referencing a
nonexistent method parameter. This compile does not verify native Windows APIs,
installed endpoint behavior, request uploads, or cross-user teardown.

The Mac package test was also attempted with a scratch path under `/private/tmp`;
SwiftPM could not compile the package manifest because the sandbox rejected its
`sandbox-exec` invocation. The Mac file-request client and UI remain unbuilt in
this environment.

## 2026-10-21 source update: combined Manager file-job targets

When the Files filter is set to All servers, the Qt Manager now gathers
enrolled computers only from currently signed-in deployments, verifies the
file-job feature on each owning server, and presents a combined recipient
picker labeled with server identity. It caps the combined selection at 50,
checks recipient sessions sequentially, and confirms signed account labels
before dispatch. Selected targets are partitioned by deployment and submitted
as independent server-local jobs; each target group carries that deployment's
session generation and pinned origin. The job history remains combined and
shows the separate server-owned job rows. An offline or expired deployment,
unsupported feature, or unavailable recipient is listed as skipped while
eligible deployments continue. The operator can also filter to one server.

This is source-only. The Qt 6.10 package remains unavailable in this
environment, so the Manager was not compiled or exercised. No tests were run.
`cmake -S apps/manager -B /private/tmp/helicoid-manager-build` stopped at
`find_package(Qt6 6.10 REQUIRED)`. Repository diff checks and a trailing
whitespace scan passed. Independent server request outcomes, cross-server
failure recovery, installed endpoint behavior, and V48/S06 acceptance remain
unverified.

## 2026-10-21 reverse file-request HTTP lifecycle coverage

The existing endpoint-enrollment integration scenario now also exercises the
reverse file-request routes: an authorized manager creates and publishes a
request, the signed-in endpoint lists it and uploads a file in a bounded
chunk, the server verifies the final digest, and the manager reviews and
downloads the received content before explicitly deleting its retained bytes.
The same user explicitly confirms a replacement upload; only after its digest
verifies does the prior submission become superseded and its bytes unavailable.
The scenario also checks manager-token denial on endpoint routes and
endpoint-token denial on manager review routes. The route inventory test
confirms the corresponding network-router endpoints require authentication.
It then signs in a second OS user on the same computer using the same display
name. The fresh session revokes the prior endpoint token, receives no rows
for the first user's submissions, and cannot upload into or replace the first
user's received submission.

`cargo test --offline --test http_api
endpoint_enrollment_requires_proof_then_explicit_epoch_fenced_approval --
--exact` passed (1 test). This exercises the in-process application router;
it is not a TLS socket, desktop upload, installed deployment, retention, or
full V48/S06 acceptance result. It covers the server-side same-computer user
switch and submission fence, but not the client OS-session detection/teardown
or filesystem ACL and path race. The Qt Manager and Mac endpoint client remain
unbuilt in this environment, and the Windows compile/test-host limitations
above still apply.

The targeted network-router route test also passed in this turn:
cargo test --offline --test http_api
network_router_exposes_v2_and_health_but_not_legacy_v1 -- --exact
(1 test). It confirms the request routes are mounted on the network router
and reject unauthenticated requests.

`swiftc -frontend -parse` passed on the edited Mac endpoint client, Computer
status model/app, and endpoint-client tests. This is syntax-only evidence; it
does not type-check against the package modules or replace the blocked SwiftPM
build/test attempt above.

After the reverse file-request and same-display-name user-switch coverage was
added, the complete in-process HTTP integration target passed:
`cargo test --offline --test http_api` (17 passed, 0 failed). This includes
the focused lifecycle and network-route checks above. It still exercises the
in-process router rather than a listening TLS server or any installed client.

## 2026-10-21 current macOS source build

`CLANG_MODULE_CACHE_PATH=/private/tmp/helicoid-clang-module-cache swift build
--disable-sandbox --scratch-path /private/tmp/helicoid-swift-build-current`
completed successfully and built the current `HelicoidCore`,
`HelicoidManager-product`, and `HelicoidComputer-product` targets. The first
attempt without `CLANG_MODULE_CACHE_PATH` failed because Swift tried to write
its clang module cache under the restricted user cache; redirecting that cache
resolved the build failure. This is a source build only: no tests, installed
application, endpoint transfer, capture, preview, or control path was run.
The Windows platform build could not start because `dotnet` is not installed.
