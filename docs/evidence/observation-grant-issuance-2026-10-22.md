# Observation admission grant source evidence — 2026-10-22

> Historical implementation notes below describe the state when each section
> was recorded. The latest source continuation is recorded at the end of this
> file and supersedes earlier statements about the Qt Manager and Windows
> Computer call paths. Capability bits remain disabled until native builds and
> installed acceptance pass.

## Implemented

The server now issues a 10-second Ed25519-signed observation grant through
`POST /v2/admissions/observation`. The grant is bound to the deployment,
account, approved Manager identity and public key, computer, enrolled endpoint
public key and session, enrollment epoch, and the `observe` capability. The
Manager request is checked against the current operator session and current
per-computer observe assignment. Issuance also requires a current approved
key-bearing Manager session and a fresh endpoint session for an enrolled
Windows or macOS computer with a signed-in local user.

The deployment-local signing seed stays in the owner-private SQLite store. The
server persists only a hash of each grant token, applies retention, row-cap,
and renewal limits, and exposes an endpoint-authenticated one-use redemption
route. Redemption validates the grant and the Manager's endpoint-nonce proof,
then rechecks both sessions, enrollment epoch, and current observe access in
the consume transaction. Restore clears outstanding grant records and rotates
the signer before the restored store is published. The schema is version 17.

macOS Swift and Windows C# clients now fetch the endpoint's current signed-in
session, verify a Manager-delivered grant against the enrollment-pinned server
key, create a fresh endpoint nonce challenge, validate the Manager's proof, and
redeem the grant with the exact endpoint bearer session. Pending challenges are
one-use and short lived in endpoint process memory. Each client also rejects a
second challenge for the same grant during its lifetime. Both clients reuse the
current authenticated endpoint session rather than refreshing it during normal
polling, because a newly issued endpoint session invalidates grants bound to
the prior session. The macOS actor coalesces overlapping session handshakes;
otherwise concurrent requests could each create a session and revoke the
other. Session completion returns the opaque session ID, and authorized
inventory exposes the current ID alongside its expiry for future Manager
correlation. A changed server key does not silently replace the enrollment pin.

The retained macOS Computer target now has a ScreenCaptureKit adapter that
requires a successfully redeemed in-memory admission lease. It captures only
the primary display, outputs BGRA without audio, caps frames at 1280×720 and
15 fps, and bounds queued frames to two. A monotonic deadline, current console
user, caller-supplied unlocked-console probe, and current Screen Recording
permission gate every delivered frame; a 250 ms watch stops the stream when
any lease condition changes. The unlocked-console probe defaults to denial,
and no native provider is connected. Apple's documented standard Window
Server Session properties expose on-console and login-complete state but no
lock-status key ([Apple documentation](https://developer.apple.com/documentation/coregraphics/window-server-session-properties)). The current adapter therefore cannot start capture by default. Expiry, user change, unlocked-state loss, permission loss, stream failure, and explicit stop close the gate and stop capture. This adapter exposes local frames to its caller; it is not yet connected to the endpoint WebRTC sender or Manager viewer.

The Windows endpoint's redeemed admission is now a non-forgeable process-local
lease: its constructor is internal, it uses a monotonic deadline, checks the
current signed-in SID and unlocked physical-console eligibility, and caps the
active lease registry at eight. Explicit end and client shutdown dispose the
lease. This object is not yet consumed by the metadata-only CaptureWorker.

## Short-lived signaling relay

The server now exposes Manager and endpoint offer/answer/ICE signaling routes
under the exact observation grant. Every publish and poll revalidates the
current Manager and endpoint sessions, endpoint incarnation, and `observe`
scope. The bounded mailbox is process-local, expires with the grant, caps each
message at 64 KiB for SDP or 4 KiB for ICE, and caps each grant at 128 messages
and 256 KiB. It is not persisted; a server restart drops pending signaling.
SDP and ICE values are visible to the authority over HTTPS. This supplies
signaling carriage only; the Manager and endpoint do not yet invoke these
routes or establish a native peer connection.

## Verification

- `cargo fmt --all -- --check` passed.
- `git diff --check` passed.
- `cargo clippy --locked --offline --all-targets -- -D warnings` passed.
- Focused admission tests passed: canonical/domain-separated signing,
  single-use redemption/replay rejection, and observe-scope revocation before
  redemption.
- Router, v14 migration, and restore-fencing tests passed.
- `swiftc -frontend -parse` passed for the observation-grant verifier and
  endpoint enrollment client (syntax only).
- Direct Rust verification after exposing the current endpoint session ID
  passed: the focused v2 inventory unit test and endpoint enrollment/session
  router test each passed (1/1); `cargo fmt --manifest-path modules/server/Cargo.toml -- --check`
  passed.
- Direct `swiftc -typecheck` passed for the endpoint client and grant verifier
  with minimal HTTP transport and auth-codec shims plus the real error and
  health DTO sources. This checks Swift types and actor/task isolation, but it
  does not build the package or execute its test target.
- A macOS regression case now races two preparations of the same grant and
  requires one challenge and one endpoint session issuance; it passed in the
  focused endpoint enrollment suite (6/6). SwiftPM needed sandboxing disabled
  and its compiler module cache redirected to `/private/tmp` in this environment.
  `dotnet` is unavailable, so the Windows C# source was not compiled.
- The Windows admission-lease source change is newer than that verification and
  remains uncompiled because `dotnet` is unavailable in this environment.
- `CLANG_MODULE_CACHE_PATH=/private/tmp/hel-swift-module-cache swift build --disable-sandbox --cache-path /private/tmp/hel-swiftpm-cache --package-path Sources`
  passed for the retained macOS 13+ Computer package, including the new
  ScreenCaptureKit adapter and default-deny unlocked-console gate. This is a
  compile result; no native unlocked-state provider, live TCC capture, or
  desktop-change runtime was exercised.
- The full `cargo test --locked --offline` run passed the library tests
  (87/87), main tests (7/7), auth tests (62/62), HTTP tests (18/18), installer
  tests (57/57), installer-adapter tests (29/29), package-target tests (26/26),
  installer-review tests (13/13), and legacy-MFA tests (7/7). It then stopped
  in `review_findings`: `health_on_loopback_socket_still_works` could not bind
  `127.0.0.1` because this sandbox returned `Operation not permitted`; 21 other
  tests in that binary passed. Later test binaries were not reached.

### Signaling relay continuation — 2026-10-22

- `cargo fmt --manifest-path modules/server/Cargo.toml` passed.
- `cargo check --locked --offline --manifest-path modules/server/Cargo.toml` passed for the grant-scoped signaling routes and in-memory relay.
- `CLANG_MODULE_CACHE_PATH=/private/tmp/hel-swift-module-cache swift build --disable-sandbox --cache-path /private/tmp/hel-swiftpm-cache --package-path Sources` passed for the macOS endpoint signaling client methods.
- No tests were run for this continuation. The Windows endpoint signaling methods remain uncompiled because `dotnet`, `csc`, and `mcs` are unavailable.
- This source work does not show that either client invokes the signaling methods, that a WebRTC peer connects, or that pixels reach the Qt viewer.

### Endpoint grant dispatch continuation — 2026-10-22

The server now queues a short-lived copy of each issued grant in a bounded,
process-local mailbox keyed by the exact endpoint session in the signed claims.
The database continues to store only the token hash. The new authenticated
`GET /v2/endpoints/admissions/observation/pending` handler subscribes before
checking the mailbox, then holds for at most 10 seconds so an issue/subscribe
race cannot lose a wake. Every read rechecks the current endpoint session. A
poll drains up to eight grants once; queues cap at eight grants per session and
512 total. Capacity refusal deletes the just-created, undisclosed grant. A
server restart drops queued copies and requires fresh issuance.

The Windows Computer now runs a cancellable poll loop after enrollment. It
checks `/health.features.screen_observation` first and waits without contacting
the grant mailbox while the feature remains disabled. Once enabled, it uses the
current endpoint session and enrollment-pinned grant key, verifies each
received grant, creates a fresh nonce challenge, and publishes that challenge
into the grant's signaling mailbox. The signaling order is now endpoint
challenge, Manager offer, endpoint answer, then ICE; duplicate challenges and
offers before a challenge are rejected. The server parses the challenge with
unknown fields denied and binds its computer ID, endpoint-session ID, expiry,
and 32-byte lowercase-hex nonce to the signed grant before queueing it.

- `cargo fmt --manifest-path modules/server/Cargo.toml` passed.
- `cargo check --locked --offline --manifest-path modules/server/Cargo.toml` passed.
- `cargo clippy --locked --offline --manifest-path modules/server/Cargo.toml --lib -- -D warnings` stops on a `clippy::type_complexity` warning in `src/service.rs`'s latest-session query, outside this continuation's changed lines.
- The same Clippy invocation passed with only `clippy::type_complexity` allowed; no other warnings were reported.
- Windows C# compilation was unavailable because `dotnet` is not installed. No tests were run for this continuation.
- The Qt Manager still does not request grants or consume endpoint challenges; neither client establishes the native peer, redeems a grant, or delivers frames. `features.screen_observation` stays disabled.

### Verified-channel proof continuation — 2026-10-22

Observation redemption now requires a canonical native WebRTC channel-binding
tuple. The Manager proof format is v2 and signs the endpoint nonce plus the
lowercase-hex binding. The server decodes the fixed-size tuple with
`hs_core::WebRtcChannelBinding` before signature verification. The macOS and
Windows endpoint APIs now accept a live-binding exporter callback, validate
the tuple and Manager proof, and require the exporter to return the same
binding immediately before redemption, after the response, and during each
admission-current check. The macOS capture gate uses that check for frame
delivery and its 250 ms monitor. The endpoint-to-native verified-handle call
path is not wired yet; callers must pass an exporter from the native
verified-channel handle. No runtime capture or peer integration is claimed.

- `cargo fmt --manifest-path modules/server/Cargo.toml` and offline `cargo check` passed for the server proof-format change.
- `CLANG_MODULE_CACHE_PATH=/private/tmp/hel-swift-module-cache swift build --disable-sandbox --cache-path /private/tmp/hel-swiftpm-cache --package-path Sources` passed after the live-binding exporter change.
- `git diff --check` passed. A duplicate Windows `EndpointObservationAdmission` record stub was removed so the concrete lease class is the sole declaration.
- No tests were run for this continuation. Windows C# remains uncompiled because `dotnet`, `csc`, and `mcs` are unavailable.
- The Qt Manager still has no peer/session loop to export and sign its native binding; this does not establish an admitted peer or deliver pixels.

## Acceptance boundary

This is still authorization and local-capture groundwork, not an admitted
preview. Endpoint clients expose challenge/redeem and pending-grant signaling APIs, but the Qt
Manager does not yet issue grants, use the signaling routes, or establish a peer connection. The native
WebRTC bridge still returns `HS_STATUS_UNAVAILABLE` for admission, capture, and
frame acquisition, and the Qt viewer has no live preview consumer. The
`screen_observation` feature remains disabled. No live capture runtime, pixels
delivered over a peer, endpoint denial, lease-expiry behavior, user-change
teardown, input, or installed-platform denial behavior is proved by this work.
The macOS capture adapter has compile evidence only; it has not been exercised
against live pixels, permission revocation, user switching, or stream failure.
Windows capture is still absent, and no remote teardown was proved.

### Qt Manager and Windows Computer source integration — 2026-10-22

The Qt Manager source now requests and verifies a server-signed observation
grant, polls its grant-scoped challenge mailbox, verifies the endpoint
certificate attestation, pins the endpoint certificate, signs and relays the
Manager certificate attestation with its SDP offer, and handles the returned
answer and ICE candidates. It checks the native channel binding, signs and
sends the Manager proof over the verified product channel, begins the local
short media lease, and displays acquired frames. Once a first frame arrives,
the source tears down the expiring peer and requests a new server grant to
continue the preview. Each period therefore receives fresh server authority.

The Windows Computer source creates a send-only native peer before grant
challenge publication, verifies and pins the Manager certificate before
setting the offer, returns its answer and ICE, verifies the proof against the
live channel binding, redeems it with the current endpoint session, then
captures a bounded primary-display frame only while the signed-in user,
physical console, channel, and lease remain current. The endpoint grant loop
handles up to eight pending grants concurrently. It performs native capability
preflight before consuming a queued grant.

These are source call paths. The native capability mask still disables
observation, the Windows DLL is absent from this checkout's retained
inventory, Qt 6.10 is unavailable, and the .NET SDK cannot load its
`Microsoft.AspNetCore.DeveloperCertificates.XPlat` assembly. The new C# path
and full Qt Manager remain uncompiled here; there is no installed pixel,
denial, expiry, revocation, or teardown acceptance evidence. No tests or builds
were run for this continuation.

### Manager grant-verifier source continuation — 2026-10-22

The shared Qt Manager now has a bounded Ed25519 verification entry point in
its native-auth ABI and a source-level verifier for the server's canonical
signed observation token. It verifies the profile-pinned deployment key,
domain-separated signature, exact claim shape, account, deployment, Manager
device and key, computer, observe capability, enrollment epoch, endpoint
session, and short expiry. The proof builder checks the canonical native
WebRTC channel-binding tuple and signs the endpoint nonce together with its
lowercase-hex binding using the Manager's protected platform identity. Grant
tokens are held by a move-only object that clears its owned token buffer on
destruction.

- `cargo fmt --manifest-path apps/manager/native-auth/Cargo.toml` passed.
- `cargo clippy --locked --offline --manifest-path apps/manager/native-auth/Cargo.toml --lib -- -D warnings` passed.
- `cargo build --locked --offline --release --manifest-path apps/manager/native-auth/Cargo.toml` passed.
- `git diff --check` passed; a direct whitespace scan found no trailing whitespace in the new Manager verifier and signing files.
- The Qt Manager was not compiled: Qt 6.10, qmake, and Ninja are absent from this environment.
- No tests were run for this continuation. The new verifier is not yet called by a Manager request/session loop; grant issuance, signaling, native peer setup, frame delivery, and live preview remain unconnected.

### Native media build reassessment — 2026-10-22

The local WebRTC scratch checkout was re-inspected before attempting a media
bridge build. Its inventory records an earlier arm64 shared-library path, but
that dylib and the harness are absent from the current `src/out/out-arm64`
tree. The checkout also lacks `build/config/gclient_args.gni` and
`tools/clang/scripts/update.py`, which the pinned build script requires before
compiling. The current checkout therefore does not provide a native media
build artifact or a complete reproducible build input set. The old inventory
entry is historical evidence only; it is not proof of a currently usable
bridge.

### Pinned dependency recovery attempt — 2026-10-22

An isolated checkout was prepared from the exact WebRTC source revision and
`gclient sync --nohooks --no-history --revision src@d7862361c6c3942b5b96c8162ae4fa37666a2ea3`
was started to restore only pinned dependencies. The sync entered CIPD ensure
for `src/third_party`; that tree grew past 8 GiB while the host had 1.6 GiB
available. The sync was interrupted before completion and the isolated partial
checkout was removed to restore disk space. The original pinned scratch tree
and workspace source were preserved. No native bridge build or harness run
occurred; the missing dependency inputs and live media path remain unverified.

### Manager native-library ABI gate — 2026-10-22

The Qt Manager source now has a small dynamic-loader boundary for its bundled
WebRTC library. Production derives the library path from the running app:
Windows uses the executable directory and macOS uses the app's `Contents/Frameworks`.
The explicit-path loader exists only in the tests and rejects relative paths.
The runtime requires ABI version 3, the connection capability bits, and every
ABI export associated with engine initialization, negotiation,
verified-channel evidence, and draining. Observation is reported supported
only when capture, admission, and media capability bits and their exports are
all present. Loading only resolves metadata; it does not initialize an engine,
issue grants, capture, or deliver frames. Peer wrappers expose product-channel
creation, bounded offer/answer and ICE handling, certificate pin/export,
verified-channel binding export, and bounded product messages. The viewer does
not use these wrappers to initialize an engine or create a peer, and there is
no offer/answer loop.

The runtime keeps the native library mapped if peer drain or engine shutdown
cannot prove teardown. A peer drain can be retried while its wrapper remains
alive; after a failed final teardown, code mapping is retained until process
exit so delayed callbacks cannot call unloaded native code. The fake ABI covers
a first peer-drain timeout followed by a successful retry and an engine
shutdown timeout. The runtime and registry tests passed (2/2) after this
lifetime fix.

The tests-only CMake target was configured and built with
`-DHELICOID_MANAGER_REGISTRY_TESTS_ONLY=ON`. Its fake shared libraries cover
missing library, relative and empty paths, ABI mismatch, incomplete capability
mask, missing capability export, and a missing negotiation export. The runtime
loader also owns initialized engine and peer handles with explicit drain,
shutdown, and safe move behavior. The fake libraries check engine/peer
creation and retirement, wrapper bounds, output clearing, and unavailable
native operations. The capability-query failure and inconsistent observation
exports also fail closed. The runtime loader and Manager registry tests passed
(2/2). These fake-library tests do not exercise the pinned WebRTC binary, the
Qt application, or a live session.
