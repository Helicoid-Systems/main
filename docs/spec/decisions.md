# Decisions and remaining questions

Updated on 22 October 2026. The user's latest direction is a general monitoring product with no room/class/lesson model. That correction supersedes the earlier classroom workflow, even where an earlier answer used classroom wording.

The accepted UI00 full74-family native redesign supersedes earlier future-module framing for M22–M25 and A11: those screens are now specified in [ui.md](../ui.md) and ordered in the acceptance phases. It also supersedes the earlier RustDesk adoption and public-routing drafting: the selected engine route is native Google WebRTC with the organization-owned public edge in [ADR 0007](../adr/0007-native-webrtc-and-public-edge.md). Historical ADRs and the RustDesk laboratory remain historical evidence and are not an alternative route to re-enable.

## Confirmed product decisions

| ID | Decision | Consequence |
| --- | --- | --- |
| D01 | Native platform-specific Computer apps: Swift on macOS and WinUI 3 on Windows; one shared Qt/SARibbon Manager | Computer status, endpoint rendering, and OS integration remain native to each platform; the Manager UI is shared |
| D02 | Shared Rust engine | Remote machinery and common contracts are shared behind native wrappers |
| D03 | Open source compatible with RustDesk | Exact license expression and redistribution obligations need the adoption audit |
| D04 | LAN first on administrator-managed computers | The initial school-owned deployment remains supported without requiring school structure or internet access |
| D05 | main coordinates separate repositories through submodules | Each implementation repository also builds independently |
| D06 | Simple UI, Apple design guidance, no text dot separators | Plain labels, native hierarchy, contextual controls, details on demand |
| D07 | Keep the broader tools without classroom concepts | Version 1 includes monitoring, assistance, sharing, attention, messages, files/file requests, app launch, screenshots, power actions, deployment, updates, and recovery |
| D08 | Live previews start automatically when the authorized console opens | Sign-in and per-computer admission precede frames; no Start Monitoring gate on the ordinary path |
| D09 | Only an authorized operator or IT explicitly stops managed remote control | The earlier teacher/IT answer maps to these general roles; automatic security teardown remains required |
| D10 | Organization-managed local accounts first | Native sign-in, account administration, MFA, and recovery work without an external identity provider |
| D11 | Intel Macs required alongside Apple Silicon | Both Mac architectures require real build, capture, input, installation, and update evidence |
| D12 | Machines are preconfigured | Enrollment and explicit per-computer access determine eligibility; discovery cannot grant access |
| D14 | Files live on the self-hosted server with quotas and retention | Verified uploads, delivery, collection, retrieval, retention, and backup are server responsibilities; committed jobs can continue after the sending console closes |
| D16 | General monitoring system, without rooms or classes | Computers is the primary view; remove room, seat, class, course, lesson, roster-participant, and teaching-owner dependencies |
| D17 | Searchable grid/list with optional simple groups | All Computers works immediately; flat named groups filter existing inventory without a required hierarchy |
| D19 | Multiple authorized viewers, one controller | Observation sessions coexist independently; control has one owner; one operator's pause cannot stop other viewers |
| D20 | Management computers need one-time administrator approval | Sign-in remains simple on an approved manager; a new, pending, or revoked manager gets no computer access |
| D22 | Computer management should follow Veyon's workflow | Include configured computers/groups, monitor in a grid, act on selected tiles or the displayed filtered set; native presentation and explicit control remain required |
| D23 | Supplied HS assets represent both company and product | Preserve the layered icon and light/dark-surface SVG logos; use the shared identity in native app branding; exact placement and platform exports follow the UI contract |
| D24 | Minimizing a remote-control window releases control and keeps viewing | Stop forwarding/release held input; release or cancel its control claim; keep separately authorized observation active; restoration requires explicit Take Control |
| D25 | Overview previews show the primary display | Resolve the endpoint's current primary display from its actual layout/capabilities; remote windows have a display picker; unavailable display shows an honest state |
| D26 | Version 1 monitors only the desktop on the computer's own screen | Select the signed-in physical/primary console session deterministically; RDP, disconnected/background, and separate virtual sessions are unsupported; no session picker or fallback to another user |
| D27 | The computer user's local keyboard and mouse remain enabled during remote control | Local and remote input may interact; no block-input toggle; only focused remote-canvas input is forwarded; local input does not grant product Stop Control authority |
| D28 | Use the supplied HS Manager App icon.icon for the Mac manager | Preserve all four original files and the layered appearances; role-specific manifest assignment; native Mac target integration/fallback must pass V51 |
| D29 | Queued sent files wait for the originally intended signed-in user to return | Bind delivery to the verified local user and enrolled computer before dispatch; pause on logout/user change; resume only for that same revalidated user before expiry; no shared-inbox or next-user fallback |
| D30 | Only the sharing operator or authorized IT can stop a receiving presentation | The computer user has no product Close/Stop Presentation command; scoped owner/IT stop and automatic security/source/desktop teardown remain required; Attention is separate |
| D31 | Local macOS server hosting joins Linux SSH and Windows-local hosting in the shared Manager | The macOS build offers This Mac, the Windows build offers This Computer, and both offer Linux SSH; the same installer/readiness/recovery evidence applies to each route |
| D32 | Public access uses organization-owned rendezvous and relay | Independently pinned trust, exact peer/channel admission, bounded relay authorization and fail-closed legacy refusal precede media/input |

Windows 11 x64 and macOS 15 or newer were the baseline in the supported-computer question. Intel support does not imply that every Intel Mac can run that OS. Exact hardware and OS compatibility need proof before a public support promise.

## Implementation confirmations, 10 October 2026

- D15 display name: Helicoid, with Helicoid Systems company identity. Native role
  names are Helicoid Manager and Helicoid Computer. Durable release package and
  service identifiers still require B01.
- D18: the user confirmed the draft behavior. Reconnecting included eligible
  computers resume view-only after fresh admission; All mode includes newly
  authorized eligible inventory; explicit-ID inclusion and existing action
  snapshots never expand silently.
- D21: closing a file request refuses new submissions and retains completed
  files. Replacing a submission requires explicit confirmation.
- The user authorized creating the separate implementation repositories on
  GitHub. The user selected the Helicoid-Systems organization alongside main.

## Manager direction update, 20 October 2026

- D33: The user selected one shared C++20, Qt Widgets Manager with SARibbon's
  Office 2021 theme for Windows and macOS. This supersedes the Manager UI
  framework/navigation decisions in ADR 0001 and this draft. The macOS and
  Windows Computer apps, workers, and privileged components keep their
  platform-specific ownership until Manager feature parity. See [ADR 0008](../adr/0008-shared-qt-manager.md).

## Superseded classroom choices

| ID | Earlier answer | Current question and draft |
| --- | --- | --- |
| D07 | Full core school product for version 1 | Reconfirmed as the full general product in the table above |
| D08 | Previews begin after Start Lesson | Replaced by automatic previews when the authorized console opens |
| D13 | Preconfigured machines, not a definite late-join policy | The old room/lesson late-arrival question is superseded. Monitoring-scope reconnect/new-computer behavior must use the new model |

The general-model clarification round is answered. Pending classroom questions cannot be used to restore a lesson or room object.

## Other product choices still open

| ID | Choice | Boundary |
| --- | --- | --- |
| D15 | Durable release package and service identifiers | Helicoid product name and Helicoid Systems identity are confirmed; final package IDs and service identities require B01 |
No product choices in this register currently await an answer. Engineering
identities and dependency/platform proofs remain prerequisites.

An operator stopping their own monitoring never ends another operator's independent observation. Discovering a new address never adds authority. Existing action snapshots never expand when a computer reconnects or is enrolled.

The concrete native adaptation of D22 is specified in [ui.md](../ui.md). Veyon's source behavior and deliberate differences are recorded in the [research ledger](../research-2026-10-09.md#veyon-source-review-computer-management). Source inspection does not establish our platform or security implementation.

## Engineering decisions requiring proof

| ID | Question | Recommendation or current position | Required output |
| --- | --- | --- | --- |
| E01 | Engine adoption | Native Google WebRTC selected; RustDesk route superseded and retained as historical laboratory only | Exact upstream commit, license inventory, native bridges, non-bypassable admission, preview/fan-out report against the WebRTC route |
| E02 | Coordination server and permission ownership | Rust server, SQLite, bounded endpoint authority | Recorded design, administrative/delegation ceilings, scoped transactions, conflict arbitration, required durable commit/failure recovery, and single-active authority cutover; V41/V46/V47/V49 |
| E03 | Protocol and manager ABI | Generated bounded contracts and a narrow C ABI | Versioned schemas/headers, ownership/threading/error rules, Swift and C# wrappers |
| E04 | Capture, input, attention, and permission behavior | Evaluate engine paths and measured native adapters under confirmed D26/D27 | Actual Mac/Windows devices, both Mac architectures, console/session selection, canvas/local input routing, user/permission changes, receiver policy, protected-desktop limitations; V40/V42/V50 |
| E05 | Host and installer paths | The shared Qt Manager offers Windows-local hosting from its Windows build, macOS-local hosting from its macOS build, and Linux SSH from both; no remote Windows installation route; containers optional; Server Ready for Enrollment ends setup and is distinct from Deployment Verified | Route inspection for both Qt Manager builds, occupied-host install/repair/update/remove evidence, V39 checkpoints in S01–S03, and product-owned resource manifest |
| E06 | Numeric limits and workloads | Bounded queues with proposed budgets in the plan | Reference hardware, measurements, frozen limits, overload behavior, boundary tests |
| E07 | Authentication and cryptography | Maintained implementations | Selected libraries/formats, scoped delegation/recovery-only reset, exact bootstrap manager binding, incarnation/connection fencing, quarantine/revocation/MFA, independent missing-history recovery and authority transition/checkpoint with documented clone limits; V43/V44/V46/V47 |
| E08 | Release identity and artifacts | Pinned sources plus signed compatibility manifest and persistent security floor | Package/service identities, signing/notarization, trusted release verifier/checkpoints, architecture, safe upgrade/rollback, role-specific Mac manager icon compilation and older-OS fallback; V45/V51 |
| E09 | Grant-signing key custody | Current implementation default: deployment-local Ed25519 key for offline LAN use | Protect the private server store, publish only the public key, rotate authority on restore, and verify host-specific protection and recovery. Revisit external HSM/KMS only if deployment requirements call for it |

Investigate codecs, cryptographic implementations, OS APIs, and IPC sizes. Ask the user when a choice changes behavior, privacy, deployment obligations, cost, or promised support scope. Never invent a dependency export or mark an engineering assumption as measured proof.

The [remote desktop integration plan](remote-desktop.md) still records the historical RustDesk specialization into RD01–RD12 and is retained unchanged as historical planning. The accepted implementation route is native Google WebRTC with the organization-owned public edge ([ADR 0007](../adr/0007-native-webrtc-and-public-edge.md)). E01 remains Needs Proof against that selected route. Exact peer/channel binding and endpoint enforcement must pass before media is enabled; failure stops adoption rather than silently selecting another transport. Actual cryptographic and ABI formats still require proof.
