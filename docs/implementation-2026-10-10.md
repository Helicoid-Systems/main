# Implementation run, 10 October 2026

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
| Core | 79 domain tests, format and clippy pass; native header drift check and isolated 61-test authority proof pass | core BUILD-REPORT and [CI](https://github.com/Helicoid-Systems/core/actions/runs/38056845742) |
| Server | 39 tests on Ubuntu and Windows; Windows release package and PowerShell bootstrap fixture pass | server BUILD-REPORT and [CI](https://github.com/Helicoid-Systems/server/actions/runs/38054445029) |
| macOS | 77 tests; Apple Silicon release packaging, Intel source compile | macOS BUILD-REPORT and [CI](https://github.com/Helicoid-Systems/macos/actions/runs/38053246915) |
| Windows | 171 tests; both x64 WinUI apps build and publish; launcher forwarding and positive native desktop query pass | Windows BUILD-REPORT and [CI](https://github.com/Helicoid-Systems/windows/actions/runs/38056087268) |
| Integration | 21 actual loopback checks, both client libraries | [sanitized result](evidence/loopback-integration-2026-10-10.json), reproducible `scripts/live-integration.sh` |
| Native Mac navigation | Packaged Manager opens; setup advances and blocks at unavailable actual SSH verification | coordinator CUA inspection; no SSH connection/deployment performed |

There are 366 passing foundation tests across the four repositories. This count
excludes the 61 isolated authority tests and engine laboratory probes, and does
not imply completion of S01–S10.
Both client contract copies match server SHA-256
`09dbc30f7dd39dc051ba037aa3059caca7bed92a6f78ce0a5e2d4c6a9ef75b2e`
(11,834 bytes).

The default clients keep production connection closed. Explicit development
mode accepts only loopback, does not follow redirects, bounds complete response
bodies and deadlines, rejects malformed authority fields and clears rejected
sessions. Inventory does not fabricate online status or pixels. Both native
apps truthfully expose unavailable remote engine states.

## Downloadable app checks

[Windows x64 development bundle](https://github.com/Helicoid-Systems/windows/actions/runs/38056087268/artifacts/11671357036)
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

The reviewed core/authority-lab candidate passes 61 tests. The coordinator
reproduced and fixed additional required-read, format-marker and intent-bound
failures. Its Ed25519 credentials have a 30-second lifetime cap, committed
issuance and atomic replay consumption. It requires a local issuance row in
the same store; peer possession, distributed admission, actual OS generations,
independent rollback anchoring and approved recovery remain unproved.
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
cannot establish the full V40 installed console/RDP/lock matrix.

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
