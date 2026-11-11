# Native video primitives — 2026-10-22

## Implementation

`modules/core/webrtc-native/native/hs_webrtc_bridge.cpp` provides bounded
send-only and receive-only video track setup, BGRA-to-I420 input, I420-to-BGRA
latest-frame delivery, current-channel-bound observation leases, and capture
generation fencing. The Manager native wrapper owns and clears acquired frame
buffers. Swift and C# bindings expose the new ABI. The native capability mask
still leaves capture, admission, and media disabled.

The native two-peer harness now covers a synthetic encoded/decoded frame,
channel-binding mismatch denial, lease expiry, revocation, and output-buffer
clearing. This is primitive-level harness coverage with synthetic authority,
not server-grant, endpoint-capture, or installed product acceptance.

Windows, the Rust server, and Qt Manager share a device-signed
certificate-attestation transcript. It binds the exact PEM bytes through
SHA-256 to the deployment, grant, computer, endpoint session, enrollment epoch,
endpoint nonce, and `manager` or `endpoint` role. The server rejects an invalid
endpoint certificate signature before relaying an attested challenge. The Qt
viewer requests and verifies a short-lived grant, polls only that grant's
mailbox, checks the endpoint challenge and certificate signature, pins the
endpoint certificate, signs the Manager certificate, and relays it in a
session-level SDP attribute. Its receive path polls answer and ICE signals,
checks the current native channel binding, sends the Manager proof, starts the
bounded local media lease, and displays acquired frames. The Manager native
runtime must advertise observation capability before this path can initialize.
After it has displayed a first frame, the Manager source requests a fresh
server grant when the short lease ends and repeats the challenge and proof
exchange; each preview period has its own authorization.

The Windows Computer source now creates the send-only native peer before
publishing a certificate-attested challenge, verifies and pins the Manager
certificate before applying its SDP offer, answers and exchanges ICE, checks
the Manager proof against the current native channel binding, redeems that
proof with the server, and streams bounded primary-display BGRA frames only
while the redeemed user, desktop, channel, and lease remain current. Capture
checks the physical console before and after each GDI read and clears both the
managed frame and temporary DIB pixels. The Computer project copies the pinned
native DLL from its retained build inventory when it is available. The
endpoint handles the server's bounded batch of up to eight concurrent grants.

This is source implementation only. The native capability mask remains
disabled and the Windows DLL is not present in this checkout's inventory. The
Windows Domain and WindowsPlatform C# projects now compile for `win-x64`, but
the full Computer WinUI app and native bridge remain unbuilt. The current
product therefore still fails closed before endpoint peer setup and has no
installed Windows-to-Mac pixel proof.

## Verification

- Generated header check passed: 40 bridge exports match the committed header.
- Wrapper contract check passed: all 40 exports are represented in Swift and C#.
- Manager registry and native-runtime tests passed: 2/2 CTest targets.
- Rust server certificate-attestation test passed: 1/1 targeted test.
- Rust formatting check passed after formatting `admission.rs`.
- Windows Domain and WindowsPlatform Release build passed for `win-x64`: 0
  warnings, 0 errors. This compiles the endpoint grant, signaling, and capture
  source, but does not verify an installed Windows runtime.
- CaptureWorker Release DLL compilation passed for `win-x64` with apphost
  generation disabled: 0 warnings, 0 errors. This checks its C# source only; it
  does not produce or validate a runnable worker bundle.
- The compile exposed and fixed byte/character constant mismatches in PEM
  validation and a missing client namespace import in the endpoint native-peer
  wrapper.
- Swift wrapper typecheck passed with the generated C header.
- Native harness strict C++ syntax check passed with `-Wall -Wextra -Werror -pedantic`.
- `git diff --check` passed in `modules/core`.

At the time this evidence was first captured, the native bridge and harness had
not been linked or executed. The subsequent exact-pin arm64 build and runtime
results are recorded in the final section below. The Qt Manager configure had
stopped at `find_package(Qt6 6.10...<6.11)` because Qt 6.10 was unavailable.
The Computer WinUI app build remains unverified: its no-restore build has no
`project.assets.json`, and its Windows UI packages are not present in the local
NuGet cache. The C# platform compile used a temporary composite of the local
SDK and repair files because the SDK executable is not on `PATH` and the
original SDK directory is incomplete.

## Endpoint close-signal cleanup correction — 2026-10-22

The Windows endpoint now attempts its authenticated close signal with an
independent one-second timeout even when the observation cancellation token
has fired. A redeemed endpoint lease remains owned by the outer session scope
until that close attempt completes, so cancellation or a later exception does
not remove the active signaling context first. If the client, user session, or
channel is no longer valid, native peer teardown and lease disposal still run.

The affected Windows platform sources now compile in the Domain and
WindowsPlatform Release build. This compile does not enable the native media
capability bits or prove an installed preview.

## Shared frame validation and mailbox coverage — 2026-10-22

Added core tests for bounded BGRA shape validation, padded rows, dimension and
stride rejection, checked-size overflow, latest-frame replacement and one-time
take, rejection of a late desktop generation without replacing the current
frame, permanent mailbox retirement after clear, and pixel wiping. Retained
frame pixels now use `write_volatile` on drop, which Rust documents as an
externally observable write that cannot be elided: [Rust `write_volatile`](https://doc.rust-lang.org/std/ptr/fn.write_volatile.html).

- `cargo test --offline --manifest-path modules/core/Cargo.toml` passed: 98 tests (54 unit, 44 integration).
- `cargo fmt --manifest-path modules/core/Cargo.toml --check` passed.
- These tests prove bounded Rust frame validation and handoff state only. They
  do not prove OS capture, encoded pixels, or installed Windows-to-Mac delivery.

## Native frame buffer clearing — 2026-10-22

Added a shared C++ secure-zero helper that clears cached BGRA frames and failed
acquisition buffers through volatile byte stores. The Manager wrapper now uses
it when retiring a frame and when acquisition fails; the bridge uses it when
the remote-frame cache is retired, a stale callback is rejected, or frame
acquisition returns an error. The standalone helper check passed with strict
C++17 warnings enabled. The Manager wrapper passed a strict C++20 syntax-only
compile, and `setup-gn-target.py` passed Python AST parsing. The GN staging
script now includes both `channel_binding.h` and `secure_memory.h` with the
staged bridge sources.

These checks do not compile or execute the WebRTC bridge: the pinned checkout
is incomplete, and the product media capabilities remain disabled. The
standalone buffer check does not prove application teardown or installed pixel
delivery.

## Remaining product gate

Build and ship a pinned Windows native DLL with endpoint observation
capabilities enabled. Compile and run the new endpoint path, then prove
denial, expiry, revocation, desktop/user changes, wrong certificates, replay,
frame teardown, and safe short-lease renewal across the installed Windows
endpoint and Mac Manager before enabling capture, admission, or media
capability bits by default.

## Per-user Windows capture worker boundary — 2026-10-22

The foreground Computer process retains grant verification, certificate
attestation, WebRTC signaling, and the native media peer. It starts a separate
per-user `Helicoid.CaptureWorker` only after the native observation capability
gate and physical-console eligibility check. For each frame, Computer checks
the redeemed admission, sends one authenticated and generation-fenced
`CapturePrimaryDisplay` assignment, then rechecks the lease and launch before
reading the response.

The worker can only capture the bounded primary display into a one-request
`Local\\` memory mapping. The name contains a 128-bit random nonce; the frame
header binds that nonce, dimensions, stride, byte length, and desktop
generation. Frames are capped at 1280x720 BGRA. The worker receives no grant
token, server credential, networking operation, or input operation. The slot
and managed frame are cleared when their scopes end. A 500 ms physical-console
observer fences worker generations on lock, loss of eligibility, and return to
an eligible desktop; capture also checks eligibility before and after GDI.

Computer build and publish now stage the self-contained worker under
`CaptureWorker/`; the Windows CI workflow validates that location and stages
the complete runtime bundle in its own directory for integration-process tests.
The CI worker artifact is now described as capture-capable behind authenticated
IPC rather than metadata-only.

Local proof for this change: Domain, WindowsPlatform, CaptureWorker, and
WindowsIntegrationTests compiled for the requested Windows target/source
surface with zero warnings and errors when apphost generation and self-contained
runtime packaging were disabled. The focused IPC protocol suite passed 83/83;
the complete portable integration run passed 17 with 57 Windows-native tests
skipped; the seven frame-slot parser cases passed. The complete Domain suite
had 497 passing tests and one unrelated presentation-copy assertion failure.
The self-contained apphost build could not be completed because this host's
temporary .NET SDK lacks `Microsoft.NET.HostModel` 10.0.12. Computer's WinUI
project is still missing restore assets and Windows UI packages here.

This remains source and portable-test proof only. The Windows-native tests did
not execute on this Mac, the published worker executable was not produced here,
the Computer WinUI app was not built, the native DLL remains absent, media
capability bits remain disabled, and no installed Windows-to-Mac pixel path has
been verified. The user/desktop lifecycle, end-to-end grant teardown, and the
plan's denial/expiry/revocation acceptance matrix still require Windows and Mac
runtime evidence.

## Capture worker transport assembly boundary — 2026-10-22

The worker IPC channel, frame slot, Windows frame capture, and native desktop
identity helpers now compile into `Helicoid.WorkerTransport`. The endpoint's
HTTPS and WebRTC orchestration remains in `Helicoid.WindowsPlatform`, and the
CaptureWorker project references `Helicoid.Domain` plus
`Helicoid.WorkerTransport` without a direct WindowsPlatform reference. The
Windows CI publish step now checks the packaged dependency manifest for
WorkerTransport and rejects a WindowsPlatform dependency.

Fresh local staging with the Computer `StageCaptureWorkerForBuild` target
copied the managed worker runtime files. Inspection of
`Helicoid.CaptureWorker.deps.json` showed `Helicoid.Domain` and
`Helicoid.WorkerTransport` with no `Helicoid.WindowsPlatform` entry. That
staging used `UseAppHost=false`, so it did not produce a runnable Windows
executable or a complete self-contained bundle.

The complete portable WindowsIntegrationTests run passed 17 tests and skipped
57 Windows-native tests on macOS (74 total). A locked local-source restore
timed out after 25 seconds before dependency resolution, so the updated lock
graphs were checked as JSON and against the project references but were not
validated by a completed NuGet restore. This does not prove native Windows
IPC, installed capture, or endpoint-to-Manager pixel delivery.

## Pinned WebRTC handshake and native media harness — 2026-10-22

The native bridge now waits for asynchronous local-description completion
before returning SDP. ICE polling reports `TIMEOUT` while gathering is still
active and `EXHAUSTED` only after the gatherer completes; the harness drains
both peers through completion and checks every candidate-add result. DTLS
observer state no longer treats the transport's pre-negotiation `closed` state
as a terminal handshake failure. A close becomes terminal only after that
transport has connected, and the live connected transport snapshot can clear a
stale failure flag. The Manager treats ICE `TIMEOUT` and `EXHAUSTED` as
non-fatal queue states, and retries transient channel `HANDSHAKE` or `TIMEOUT`
results while its signed grant and session remain current.

The pinned scratch checkout passed the repository's source, `depot_tools`, and
DEPS hash preflight: WebRTC `d7862361c6c3942b5b96c8162ae4fa37666a2ea3`,
`depot_tools` `2fd34a8813b6f34336aad40976cc4223768177f8`, and DEPS SHA-256
`d060df2421433efe13a0ab4f227174676f0b07f3c10a4be13b81d8dbe761a86f`. The
arm64 target `hs_native_build:hs_webrtc_harness` linked successfully. Ninja
reported 195 warnings from the bridge compilation and 5 harness shadow
warnings; it reported no errors. The escalated local-socket run passed all 214
checks with zero failures, including
certificate-pin rejection, matching channel bindings, synthetic lease
admission and expiry, decoded BGRA frame delivery, queue clearing, peer churn,
and delayed-callback teardown.

The shared `libhs_webrtc_shared.dylib` also linked as arm64 with SHA-256
`6396185b301eeec8c327c6461e996b85fa1aea092ca6f869a10eef060becf401`.
`nm -gU` found exactly the 40 exports declared by the generated header, and a
dynamic load/query returned ABI 3, capability status 0, and mask `0x0000008f`.
That mask includes initialization, negotiation, channel evidence, drain, and
the TURN/TLS lab bit; capture, admission, and media remain disabled.
A strict C++20 probe linked to `apps/manager/src/native_webrtc_runtime.cpp`
loaded this dylib through `NativeWebRtcRuntime::loadForTesting`, reported
connection support and no observation support, and initialized and shut down
an engine successfully. This did not build the Qt UI or exercise its bundled
application path. The relay-only harness was also updated to wait for gathering
completion and linked successfully; its coturn scenarios were not run because
no relay fixture configuration was supplied in this check.

This proves the pinned arm64 bridge and synthetic two-peer harness only. It
does not enable capture, admission, or media capability bits; it does not use a
server-issued grant or physical Windows capture; and it does not prove an
installed Windows-to-Mac preview. The Manager retry change was not compiled in
this run because Qt 6.10 is unavailable in the environment.
