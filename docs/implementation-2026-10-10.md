# Implementation run, 10 October 2026

## MiMo V2.6 production prerequisites

The current wave uses installed MiMo Code with `xiaomi/mimo-v2.6-pro`.
The coordinator checks sanitized exported session metadata, reviews each diff
and repeats acceptance commands. The earlier V2.5 results below are historical
foundation evidence; they do not describe the current completion status.

The requested scope includes all four Mac/Windows manager–endpoint combinations,
Linux SSH, Windows-local and macOS-local hosting, and organization-owned public
rendezvous/relay. These are implementation targets. Live preview, control,
production routing and verified installation/recovery are still unavailable.

| Capability | Current evidence | Status |
| --- | --- | --- |
| Separate issuer/receiver admission candidate | Coordinator: 167 runtime tests and four compile-fail doctests; [native Linux/Mac/Windows CI](https://github.com/Helicoid-Systems/core/actions/runs/38123977124) passes formatting, tests and clippy; signatures, replay, restart and controller renewal exercised | Verified as an isolated candidate; actual authenticated OS/channel binding Not Verified |
| Mac “This Mac” hosting preflight | 123 Swift tests; packaged UI blocks incomplete checks; Apple Silicon app build and Intel source build pass [CI](https://github.com/Helicoid-Systems/macos/actions/runs/38085682009) | Verified preflight only; service installation and capture/input Not Verified |
| Windows metadata worker fencing | 387 domain tests (including95 pinned HTTPS checks) and47 native integration tests pass on the Windows Server 2025 CI runner; deterministic DPAPI envelope/ciphertext corruption checks exercise the actual storage seam | [Native CI38142367280](https://github.com/Helicoid-Systems/windows/actions/runs/38142367280) Verified; Windows 11 installed capture/input Not Verified |
| RustDesk bridge frame lifetime | Mac arm64 linked lifetime and Intel compile/link prerequisites; [native Windows CI38317350914](https://github.com/Helicoid-Systems/core/actions/runs/38317350914) passes32 Rust/133 C/85 C# checks plus133 C/85 C# against retained DLL; actual bundle inspected | Inert lifetime/ABI and retained Windows lab bundle Verified; Intel runtime and admitted media Not Verified |
| Selected OSS routing adoption | Stock relay global key/UUID pairing lacks Helicoid per-allocation authorization; negotiated peer/channel binding and endpoint admission are not proved | Failed adoption gate; media/control and public deployment remain closed |
| Server authentication and installer hardening | Coordinator272 Mac tests; [native CI38320441248](https://github.com/Helicoid-Systems/server/actions/runs/38320441248) passes265 Linux /273 Windows tests,26 signed-package product/version/target/trust/path tests, strict Clippy/release and both PowerShell launchers; actual download inspected | Metadata/private-store/MFA/CLI and literal-loopback HTTPS prerequisites Verified; installation/public auth/recovery Not Verified |
| Production release | Developer ID/notarization and Windows signing absent; installed OS acceptance, recovery and security review incomplete | Not Verified; release gated |

The Mac HTTPS and command-lifecycle prerequisite now passes complete [native CI 38167550626](https://github.com/Helicoid-Systems/macos/actions/runs/38167550626) at exact code `7bf00ac9ce6b03f47f66539a913136451abe55fd`: **186 tests in 27 suites** pass in both Test and Package apps, release packaging and Intel cross-build pass. The coordinator independently passed the complete concurrent suite and Apple Silicon packaging, verified all six frozen source hashes and both downloaded bundle signatures. The integration pin `ae34bc6` adds only reviewed evidence documentation. TLS fixtures use a dedicated serial native generator while awaiting test tasks suspend; actual command cancellation, timeout, rejected stdin and output overflow confirm direct-child cleanup before return. Original timing assertions and concurrent suites remain intact.

Server package identity/target verification and Windows loopback HTTPS pass complete [CI38320441248](https://github.com/Helicoid-Systems/server/actions/runs/38320441248) at code `21d2ff337e0b02c4dd01ea8a3bed5213fad3dacf`: **265 Linux and273 Windows tests**, strict Clippy/release and both PowerShell launchers. Coordinator272 Mac tests and26 signed-package tests pass. The original accepted snapshot admits otherwise-valid signed wrong-product, malformed-version and empty packages; the final regressions against that isolated baseline fail exactly3 tests while23 pass. The candidate rejects all three and preserves strict Ed25519, independent publisher trust, all four trusted targets, portable paths and compatibility checks. [Accepted identity evidence](evidence/installer-product-acceptance-2026-10-13.json) includes the verified [development server download](https://github.com/Helicoid-Systems/server/actions/runs/38320441248/artifacts/11778357880). Integration pin `44a8eccb6d1503e0f1bef1f5dcd7a8310bc28b72` changes only README/BUILD-REPORT from tested source. Installation commands remain Linux-only; Windows/macOS service setup, public hosting, updates/floors/rollback, recovery and signing remain unavailable/unverified. Windows cleanup is owned-child kill/reap, not installed service-stop acceptance. Earlier target/TLS evidence remains historical.

The [retained Windows laboratory bundle](https://github.com/Helicoid-Systems/core/actions/runs/38317350914/artifacts/11778571128) passes full [native CI38317350914](https://github.com/Helicoid-Systems/core/actions/runs/38317350914) at source `155fff24605d4ae3beacd2b91986676ff8452c72`: **32 Rust /133 C /85 C# checks**, plus **133 C /85 C# against the exact retained DLL copy**. Separate [domain/authority/header CI38317351049](https://github.com/Helicoid-Systems/core/actions/runs/38317351049) passes all five jobs. Coordinator download inspection verifies the actual x64 DLL, all28 receipt members, header,27 exports,16 frozen source hashes, original/staged reports and ZIP SHA256 `69f6dfddbb303bd7d61bca5daeea731313f4986896225925b339733782a34afe`. The portable metadata checker passes on the real downloaded bundle; Windows execution proof comes from native CI. All94 exported MiMo records identify V2.6 Pro. Integration pin `872a553bbe479041645286b1021dd309cf85f2ac` changes only reviewed documentation/inventory from tested source. [Accepted bundle evidence](evidence/engine-retained-bundle-acceptance-2026-10-13.json) records measured runtime prerequisites and proof limits. The inert unsigned receipt is integrity evidence only; normal app linkage, admitted media/input/channel binding and selected-route adoption remain unavailable. Earlier candidate/failure records remain historical.

[The current candidate ledger](evidence/native-candidate-progress-2026-10-12.json) records exact source/run pairs, coordinator checks, verified MiMo model exports and remaining gates. The new Windows server download is a private-store/MFA/CLI and loopback HTTPS prerequisite. The requested worker/admission, live-preview and control/installation checkpoints remain incomplete.

The 11 October real Rust TLS health probe also passes with the accepted Swift source and unchanged .NET-on-Mac client (0.264s and 0.351s respectively); server SIGINT exits zero. These disposable certificate/loopback measurements do not establish public authentication or fleet performance.

Exact revisions, earlier failures and remaining gates are recorded in [the candidate ledger](evidence/native-candidate-failures-2026-10-11.json). Local tests and exported MiMo metadata do not substitute for native gates.

[Current Apple Silicon development apps](https://github.com/Helicoid-Systems/macos/actions/runs/38167550626/artifacts/11711859570)
are built from `7bf00ac9ce6b03f47f66539a913136451abe55fd`. Both downloaded
archives pass ZIP integrity checks and extracted bundle signatures pass
`codesign --verify --deep --strict`. These use ad-hoc signatures. This is a
setup/HTTPS prerequisite build, not a passed worker/admission, live-preview,
or control checkpoint. Intel compilation does not establish Intel runtime
acceptance.

[Current Windows x64 development bundle](https://github.com/Helicoid-Systems/windows/actions/runs/38142367280/artifacts/11703970176)
is built from `f8f356d3f29419f1161952d38bfb6428f24b3666`; the integration pin
adds only reviewed evidence documentation at `75448f2`. The coordinator
downloaded it and verified the Manager, Computer and metadata-worker executables
as PE x64. Native CI passed all 47 integration tests without skips on Windows
Server 2025. This establishes the tested IPC/storage prerequisites, not installed
Windows 11 capture/input or the complete worker/admission checkpoint.

The v1 server contract and both vendored client copies match SHA-256
`3e9f5de05569618cc1746513e4da70e789888bca4d42b73628864e7335a1ac02`
(12,354 bytes). HTTP wire version stays 1; storage version is independently 2.

The supplied Ubuntu host has been inspected read-only. Its existing services,
data and occupied ports 80/443 remain intact. No authority, rendezvous or relay
listener was opened. Server Ready for Enrollment and Deployment Verified remain
separate states; neither follows from these development tests.


The first implementation wave uses four MiMo Code sessions with the explicit
`xiaomi/mimo-v2.5-pro` provider/model selection. Session export verified that
selection before dispatch. The coordinator owns integration and fresh checks.

| Work package | Owned source tree | Initial deliverable |
| --- | --- | --- |
| Core | `modules/core` | Typed domain state, bounded inventory/action scope, observation/control lifecycle, deterministic tests |
| Server | `modules/server` | Loopback development authority, durable local accounts, manager approval, scoped inventory, HTTP contract and denial tests |
| Mac | `apps/macos` | Independent SwiftUI manager and computer status apps, native inventory interaction, truthful unavailable-engine states |
| Windows | `apps/windows` | Independent WinUI 3 manager/status source and separately testable C# domain logic |

Agents may edit only their assigned tree. Existing plans and brand originals are
preserved. Each owner supplies independent build instructions and a build report.
The coordinator reconciled byte-identical contract copies, reviewed fail-closed
behavior and repeated affected tests and actual client integration.

## Working assumptions

The user confirmed Helicoid (Systems). Role names are Helicoid Manager and
Helicoid Computer. Final release package/service identifiers await B01.
The user authorized creating separate GitHub repositories in the
Helicoid-Systems organization alongside main. All four independent repositories are published privately in that organization
and registered as Git submodules. Integration commits are pinned in main.

The user confirmed the D18 draft: All mode can include freshly authorized
inventory, explicit-ID inclusion stays fixed, and existing action snapshots
never expand. The user confirmed D21: close refuses new submissions while
retaining completed files; replacement requires explicit confirmation. A
Windows 11 PC is available for subsequent real endpoint test builds.

## Evidence boundaries

Development domain tests do not establish production admission. The loopback
server cannot establish production LAN authentication, MFA, signed grants,
installation, recovery, or a release support promise. An unavailable remote
engine must remain unavailable in both UI and backend.

B02/B03 and RD01–RD12 remain gates. No RustDesk/native media integration,
real remote control, capture permissions, fleet benchmark,
signed installation, or full version 1 completion is claimed by this run's
dispatch. Exact completed checks and limitations will be recorded after review.

## Local tools

Observed before dispatch: Xcode 27.0 (27A266a), Swift 6.4 on Apple Silicon;
no global Rust or .NET tools. The coordinator installs temporary isolated
toolchains in `/tmp/hs-toolchain`, without modifying shell configuration.
Those temporary paths are execution conveniences, not reproducible release pins.

## Verified development foundation

| Area | Fresh result | Retained evidence |
| --- | --- | --- |
| Core | 79 domain tests; native header drift check; isolated 87 runtime and two compile-fail checks, format and clippy pass | core BUILD-REPORT, authority PROOF_REPORT and [CI](https://github.com/Helicoid-Systems/core/actions/runs/38063286220) |
| Server | 39 tests on Ubuntu and Windows; Windows release package and PowerShell bootstrap fixture pass | server BUILD-REPORT and [CI](https://github.com/Helicoid-Systems/server/actions/runs/38054445029) |
| macOS | 77 tests; Apple Silicon release packaging, Intel source compile | macOS BUILD-REPORT and [CI](https://github.com/Helicoid-Systems/macos/actions/runs/38053246915) |
| Windows | 210 tests; both x64 WinUI apps build/publish; exact role icons, launcher, native desktop/lock queries and sampler pass | Windows BUILD-REPORT and [CI](https://github.com/Helicoid-Systems/windows/actions/runs/38059359725) |
| Integration | 21 actual loopback checks, both client libraries | [sanitized result](evidence/loopback-integration-2026-10-10.json), reproducible `scripts/live-integration.sh` |
| Native Mac navigation | Packaged Manager opens; setup advances and blocks at unavailable actual SSH verification | coordinator CUA inspection; no SSH connection/deployment performed |

There are 405 passing foundation tests across the four repositories. This count
excludes the 87 isolated authority runtime tests, two compile-fail checks
and engine laboratory probes; it does not imply completion of S01–S10.
Both client contract copies match server SHA-256
`09dbc30f7dd39dc051ba037aa3059caca7bed92a6f78ce0a5e2d4c6a9ef75b2e`
(11,834 bytes).

The default clients keep production connection closed. Explicit development
mode accepts only loopback, does not follow redirects, bounds complete response
bodies and deadlines, rejects malformed authority fields and clears rejected
sessions. Inventory does not fabricate online status or pixels. Both native
apps truthfully expose unavailable remote engine states.

## Downloadable app checks

[Windows x64 development bundle](https://github.com/Helicoid-Systems/windows/actions/runs/38059359725/artifacts/11671909854)
contains both actual PE32+ executables and self-contained dependencies. The user ran both applications on Windows and reported that they work. This
confirms their launch on that PC; the full navigation and accessibility matrix
remains unverified. Follow [Windows test instructions](windows-testing.md).

[Mac app archives](https://github.com/Helicoid-Systems/macos/actions/runs/38053246915/artifacts/11670531699)
contain both Apple Silicon apps. Downloaded zip archives preserve executable
permissions; both extracted full-bundle signatures pass `codesign --verify --deep --strict`.
CI packages provenance-checked .icns fallbacks from approved artwork; local
Xcode 27 also proves layered-icon compilation. They use ad-hoc signatures; Developer ID/notarization and the full
OS/icon/accessibility matrix remain unverified. Intel source compilation is
verified, but a universal release package is not produced.

## Next implementation boundary

The isolated [engine baseline](engine-baseline-2026-10-10.md) passes its pinned
source check. RD02 remains incomplete: the initial state-only wrapper was not
accepted as real RustDesk viewer initialization. The corrected lab constructs and drops the
actual generic Session/handler without starting listeners, capture or input.
Nine Rust tests and 45 C harness checks pass on macOS arm64, and the retained
patch reproduces against a clean source checkout.
The same inert library and C harness also compile and link for Intel macOS;
execution remains unverified because Rosetta is absent on the build machine.
The apps cannot enable viewing/control until B02/B03 and installed console
admission/teardown proofs pass.

MFA, file jobs, fleet actions, production deployment, backup/restore and full
remote media/control are unfinished. These are executable follow-up packages,
not features to infer from a successful source build.

The reviewed core/authority-lab candidate now passes 87 runtime tests and
two compile-fail checks. The coordinator reproduced and fixed additional required-read, format-marker and intent-bound
failures. Its Ed25519 credentials have a 30-second lifetime cap, committed
issuance and atomic replay consumption. It requires a local issuance row in
the same store; real authenticated transport, distributed admission, actual
OS generations, independent rollback anchoring and approved recovery remain unproved.
It stays isolated from production callers. B03/RD03 remain incomplete.

[Windows development server](https://github.com/Helicoid-Systems/server/actions/runs/38054445029/artifacts/11669829225)
includes the executable, a foreground loopback launcher, and tester instructions.
Native Windows CI verified Unicode password stdin, child-only bootstrap token
environment, unchanged caller environment, and single-use bootstrap denial.
The user confirmed that the foreground server launcher works and subsequently
confirmed Manager development sign-in works on that PC.

A third continuation package owns the Windows physical-console diagnostic.
It reads WTS/desktop metadata only, keeps remote access unavailable and
provides a separate runnable tool for installed console/RDP/lock checks.
Native CI proved the real bounded desktop-name query and result shape. It
cannot establish the full V40 installed console/RDP/lock matrix. The user
reported two eligible unlocked runs, followed by an eligible result in the
reported Win+L test. That candidate's lock refusal failed; the original query
lacked an independent session-lock flag. Sanitized evidence is retained in
`docs/evidence/windows-console-user-2026-10-10.json`. The repaired diagnostic
now passes the reported unlocked-to-locked check below.
The remaining installed-session matrix is still required.
The isolated diagnostic remains disconnected from production authorization.

The user's server-test video showed the PowerShell file opening in an editor
and a later terminal disappearing before a readable error. The corrected
bundle includes a CMD launcher that retains errors without elevation or
execution-policy override. Native CI executed this actual launcher with a
controlled failing child and verified diagnostic retention and exit status.
The user subsequently reported greyed-out Manager sign-in fields. This matches
the default closed connection mode. The coordinator provided a concrete
PowerShell invocation with the explicit development switches. The user
confirmed that it works. The new bundle includes an explicit development CMD
launcher; native CI proves its forwarding and retained failure diagnostics.
This proves the development sign-in path on that PC; it does
not establish production authentication or endpoint enrollment.

The supplied Windows role icons supersede the previous Windows shared artwork:
blue Manager and dark Computer. Native CI 38058328331 and the downloaded
artifact verified exact referenced PE icon payloads, sibling ICO files and
unchanged source hashes. The user subsequently confirmed both apps open and
the icons work in the requested Explorer/taskbar check. Mac artwork is unchanged.
The lock diagnostic repair is in [Windows draft PR 1](https://github.com/Helicoid-Systems/windows/pull/1); it passes 210 portable
tests and adds independent WTS lock evidence plus a pre-started sampling mode.
Native CI 38059359725 also passed the actual extended WTS query with Unlocked
classification and all 31 sampling observations.
The user supplied the complete repaired 31-sample, 30-second Win+L report.
The initial unlocked sample was eligible; the next sample denied a desktop
query failure during transition, and the final 29 samples denied SessionLocked
despite a stable Default desktop. All 30 locked samples refused eligibility.
This passes the reported unlocked-to-locked metadata check on that PC.
The subsequent repaired-copy schema 2 report confirmed Unlocked, successful
lock query and eligibility after unlock. This passes diagnostic recovery and
the reported unlocked → locked → unlocked sequence on that PC.
RDP is supported on the tester's PC but not configured, so its runtime matrix
remains pending. This is not admission, capture/input or a full V40 proof.

The isolated authority prerequisite was merged in [core PR 1](https://github.com/Helicoid-Systems/core/pull/1)
on 2026-10-10 as `3506a1175dbd38da44072e773b797070bed6e405`.
The workspace now pins that merged commit. Its tree exactly matches the reviewed
revision `11043b3727b4efeff19fd522139bf60adee12866`. A MiMo Code agent using
`xiaomi/mimo-v2.5-pro` implemented a real Manager-key possession primitive;
the coordinator reviewed it and reproduced six deadline/grant-binding failures
before correction. The receiver creates a fresh, bounded challenge binding
the exact key, computer/incarnation/desktop, operation, intent, signed grant
and fixture channel. Foreign verifiers, replay, altered context, expiry and
clock rollback refuse proof. Verified results are sealed behind immutable
getters; caller fabrication/mutation compile-fail checks went red then green.
Fresh coordinator checks passed all 87 runtime tests, both compile-fail checks,
format and strict all-target clippy. This primitive remains separate from
same-store admission, with no production or real transport integration.
Channel and desktop-generation inputs are fixture values. B03/RD03 remain
incomplete, including distributed endpoint enforcement, durable challenge
replay/restart state, actual OS generations, recovery and authority cutover.

[Core CI 38063286220](https://github.com/Helicoid-Systems/core/actions/runs/38063286220)
passed all three jobs for the reviewed revision: main core format/test/clippy,
retained native-header drift and candidate authority format/test/clippy.
The authority job passed 87 runtime tests and both compile-fail doc checks.
