# Product and engineering plan

Planning draft updated on 10 October 2026. The product is a native, open-source computer monitoring and remote assistance system. The product name is Helicoid, from Helicoid Systems. The user's general-monitoring correction supersedes the former classroom model.

> **Manager direction update, 20 October 2026:** the user's Helicoid core v1
> plan and [ADR 0008](adr/0008-shared-qt-manager.md) supersede this draft's
> native SwiftUI/WinUI Manager framework, navigation, and platform-baseline
> decisions. The Computer applications and endpoint behavior remain in their
> platform repositories. This document continues to describe product and
> security behavior until a specific part is superseded by the new plan.

## 1. Product direction

Open the authorized management console and see live computer previews automatically. Search, switch Grid/List, or use an optional simple group. Select computers and perform a clearly scoped action. Open a computer in view-only, then explicitly Take Control when needed.

Overview previews show the primary display; detailed windows offer a display picker. Minimizing a remote window releases control while keeping authorized viewing active. Restoring requires explicit Take Control again, after any pending release is reconciled.

Version 1 monitors only the desktop shown on the computer's own screen, not separate Microsoft Remote Desktop or background-user sessions. The computer user's keyboard/mouse remain enabled during control. Only the operator's explicitly focused remote canvas forwards input; native toolbars, menus, and dialogs stay local.

The interface has one primary object: Computer. Accounts have operation permissions on computers. Groups organize inventory without creating classes, rooms, seats, schedules, or lessons. The initial deployment can remain a school LAN with school-owned managed machines; the software uses general organization/operator/computer terminology.

Mac is the priority Manager validation platform and Windows is the priority monitored endpoint. The shared Qt/SARibbon Manager builds for both Windows and macOS; the Computer applications, workers, and privileged components keep their platform-specific ownership. Internet access and a vendor account are not required for ordinary local operation.

| Confirmed constraint | Consequence |
| --- | --- |
| Shared Manager UI | C++20, Qt Widgets, and SARibbon's Office 2021 theme on Windows and macOS |
| macOS Computer app | SwiftUI/AppKit status and OS integration; capture/input stays in the signed desktop worker |
| Windows Computer app | WinUI 3 status and OS integration; capture/input stays in the signed desktop worker |
| Shared Rust engine | Shared remote machinery behind tested native wrappers |
| Open source compatible with RustDesk | Evaluate an audited RustDesk engine candidate; license direction does not prove embeddability |
| LAN first, managed devices | Administrator deployment, local trust/accounts, optional remote mode later |
| Intel and Apple Silicon Macs | Both architectures need capture/input/package/update evidence |
| main with separate implementation repos/submodules | Exact pinned gitlinks and independently buildable repos |
| Optional simple groups | Flat named sets; All Computers usable without groups |
| Automatic authorized previews | No start lesson or manual start gate; admission before every first frame |
| Broader tools retained | Full release includes sharing, attention, messages, files/requests, app launch, screenshots, power, deployment, updates, recovery |
| Managed user cannot explicitly stop operator control | Owner/authorized IT can stop; expiry/revocation/desktop changes still stop automatically |
| Local accounts first | Organization-operated sign-in/MFA/recovery; external identity later |
| Multiple viewers, one controller | Independent admitted observation; atomic control ownership and explicit conflicts |
| One-time management-computer approval | Approved device identity checked alongside ordinary account sign-in |
| Server file content with quotas/retention | Committed delivery/collection can continue without the sending console |
| Veyon-style computer management | Separate monitoring inclusion from action selection; selected-or-displayed action defaults |
| Minimal clutter and no text dot separators | Plain copy, stable computer grid, contextual results, disclosed technical details |

Windows 11 x64 and macOS 15 or newer are the current planning baseline, including compatible Intel Macs. Exact support needs actual inventory/testing; this does not promise every Intel model or Windows ARM/Windows 10.

The [decision register](spec/decisions.md), [system contracts](spec/contracts.md), [workflow contracts](spec/workflows.md), and [screen specification](ui.md) are the build contract. Detailed prose is not a claim that engine/platform proof gates have passed.

## 2. Feature scope

Veyon's documented features provide a comparison baseline, including monitoring, demonstrations, remote assistance, messages, launches, files, screenshots, and machine actions. [Veyon feature manual](https://docs.veyon.io/en/latest/user/features.html)

A pilot is an intermediate proof, not version 1 completion. The full core general product is required; advanced modules follow dedicated proofs.

| Feature | Version 1 behavior |
| --- | --- |
| Computer overview | Automatic admitted previews, flat searchable Grid/List, optional groups, readable names |
| Detailed observation | View-only separate window, display selection, fit/actual size, honest stale state |
| Assistance | Explicit Take Control, visible operator identity, one controller per computer |
| Screen sharing | Authorized local display/window or remote source, fixed receivers, Stop Sharing |
| Attention | Plain owner/message, tested temporary restriction, release/watchdog/IT recovery |
| Message/help | Bounded plain text, exact delivery results, stable help list without implicit control |
| Website/app launch | HTTP(S) browser request or IT-approved typed platform catalog |
| Send/receive files | Bounded authenticated transfer and file requests, safe names, explicit replacement/close policy |
| Screenshot | Explicit distinct permission and native export; no continuous archive |
| Power/session actions | Wake, OS lock, logoff, restart, shutdown with actual capability/results |
| Inventory/configuration | Preconfigured enrolled computers, direct account access, CSV, policy revisions |
| Deployment/recovery | Signed installers, guided local hosting, fleet updates, tested encrypted backup/restore |

Later modules: external identity/directory integration, managed app/site restrictions, audio/recording, and remote OS sign-in. Each needs a separate capability, permission, retention, and recovery contract. No ordinary navigation advertises unsupported future functionality.

Mac manager, Windows manager, Mac endpoint, and Windows endpoint share logical semantics while exposing real platform capability differences. Online, OS user signed in, capture available, and control permitted are independent. No pre-login/protected-desktop promise without proof.

## 3. Operation behavior

### Normal console path

Connect/verify a server if needed, sign in with a local account on a once-approved management computer, then Computers. Begin observation automatically only after computer-specific authorization. Restore the operator's monitoring inclusion against current authorized inventory; stale inclusion/access caches cannot grant viewing.

Single click selects; double-click opens view-only. Control needs its own admitted transition. Share Screen, Message, and Actions form the restrained ordinary toolbar. Exceptional results appear in context with a per-computer detail list. IT tools open separately through authorized commands.

Follow Veyon's distinction between computers included in monitoring and temporary selected action tiles. Optional flat group/computer checkboxes choose the included set; no group is required. New action scope is selected displayed computers, or all currently displayed computers after search/status filtering. Offline computers remain targets if displayed. Changing inclusion/search/status clears temporary selection. A dialog freezes exact IDs when opened and names its actual targets; subsequent changes cannot expand it. Group membership never grants permissions. See the pinned [source review](research-2026-10-09.md#veyon-source-review-computer-management).

Pause Monitoring ends this console's observation/control and clears pixels; Resume obtains fresh authority. Sharing/attention have distinct stop controls. Sign-out/Quit releases owned sensitive work. Closing the main window does not silently stop an independent remote window. Crashes and force termination are handled by leases.

### Endpoint experience

The tray/menu-bar app shows connection and actual active operators. Computer name is primary, optional trusted user name secondary. Viewing and Controlling have clear visible wording. A user can request help, dismiss messages, receive files, and submit to a file request.

Managed mode has no user stop-control command. Owner/authorized IT teardown is separate from screen viewing. OS authentication, secure attention, user switching, and required permission prompts retain their actual platform protections.

### Ownership

Monitoring sessions belong to operators and refer directly to computers. They do not own a room or create lesson membership. Multiple authorized viewers may observe independently; only one controller owns a computer at a time. Sharing/attention conflicts use explicit per-computer ownership rather than silently overwriting another operator.

Group actions snapshot targets and store independent results. Issued, Delivered, Applied, Unavailable, and Outcome Unknown are different. A duplicate ID with different payload is rejected. No general exactly-once promise covers an OS side effect followed by a crash.

### Failure

| Event | Required result |
| --- | --- |
| Manager crash | Stop channel/renewal and expire its authority; other owners stay independent |
| Server unavailable | No fresh operations; existing leases expire; ordinary local computer work continues |
| Reconnect | Fresh admission, no buffered input/control/destructive replay |
| Offline shutdown target | Unavailable; never queue for its next boot |
| User/desktop replacement | Stop old capture/input, clear caches, fresh new-desktop authority |
| Mac permission loss | Stop affected capture/input and name the missing permission |
| Attention worker failure | Watchdog/authorized recovery releases temporary restriction |
| Partial action | Exact original scope plus per-computer acknowledgments |
| Ambiguous OS result | Reconcile the original action before destructive retry |

Initial lease candidates are at most 30 seconds for media/control and 60 seconds for attention without renewal. Monotonic expiry, admission skew/replay rules, and maximum session lifetime need E03/E06/E07 proof. Reconnect/wall-clock movement cannot extend stale authority.

Typed input supports tested physical keys and committed text, layouts/IME, display coordinates and scaling. Release held keys/buttons on every exit. Clipboard starts off; optional text-only synchronization needs a distinct capability and loop prevention. No keystroke/clipboard-content log.

Files live on the self-hosted server with quotas and retention. Explicit destination categories, byte/hash/quota checks, safe atomic commit, and scoped pickup govern delivery. Committed server jobs and open file requests can continue after the console closes; unfinished local uploads cannot. File requests do not authorize arbitrary remote folder browsing. Closing a request refuses new submissions and retains completed files; replacing a submission requires explicit confirmation. Monitoring pause is not a file-request deadline.

### Required desktop and recovery boundaries

These contracts are mandatory implementation gates. D26/D27 are confirmed product choices; engineering details still need actual proof. D29 confirms queued delivery waiting for the originally intended user, and D30 confirms presentation stop only by the sharing owner or authorized IT. The linked system contracts own the rules, rather than engine defaults or an implementation agent's preference.

| Boundary | Required contract | Verification |
| --- | --- | --- |
| OS session selection | [Deterministic physical/primary console selection](spec/contracts.md#os-session-selection); no fallback to another signed-in user; new generation on user/lock/attachment/worker changes | V40 |
| Conflicting actions | [Complete coexist/refuse/replace matrix](spec/contracts.md#interaction-conflicts); exact owners, scoped stop authority, atomic reservations, teardown before replacement/power, no automatic restoration | V41 |
| Input ownership | [Focused canvas and enabled local input](spec/contracts.md#local-and-remote-input-ownership); local native UI never injects remote events; held injected input releases on focus loss | V42 |
| Copied enrolled identities | [Authenticated competition quarantine and fresh-key IT replacement](spec/contracts.md#enrollment-identity-competition); safe reconnect fencing; no claim of universal sequential-clone detection | V43 |
| Old backup without later history | [Recovery quarantine](spec/contracts.md#restore-with-missing-revocation-history) until complete trusted history or independent fresh IT validation; historical credentials cannot approve themselves | V44 |
| Correctly signed old releases | [Persistent security floor and separate authorized rollback](spec/contracts.md#signed-updates-and-authorized-rollback); trusted freshness/checkpoints, exact safe artifact, compatible data, no vulnerable downgrade | V45 |
| Administrative escalation | [Explicit grant/reset ceilings and independent recovery](spec/contracts.md#administrative-delegation-and-account-recovery); no implicit IT viewing, pending-manager self-approval, or normal target session from a reset; reviewed security-authority exceptions | V46 |
| Two copies of a server | [One active authority and independently trusted cutover](spec/contracts.md#active-server-authority-and-cutover); old-host fencing, fresh issuer, persisted peer adoption, honest offline/copy limits | V47 |
| Queued files after user change | [Confirmed intended-user recipient and submission isolation](spec/contracts.md#file-recipient-and-os-user-boundaries); private destination, Waiting for Recipient until the same user returns before expiry, no shared/next-user fallback | V48 |
| Required state cannot be saved | [Durable admission/effect boundaries and storage recovery](spec/contracts.md#failed-persistence-and-storage-recovery); failed writes block new work, safety teardown survives, actual consistency/probe before reopening | V49 |
| Presentation recipient behavior | [Confirmed owner/IT native receiver stop policy](spec/contracts.md#screen-sharing-receiver-policy); user dismissal denied at endpoint, honest minimized state, no stopped-branch replay, final receiver ends source demand | V50 |

## 4. UI specification

[ui.md](ui.md) describes 74 screen families and their recovery states. The main view is Computers with optional flat groups, search and Grid/List. Screens dominate the content. Native system typography/accent, quiet healthy states, contextual stop controls, and plain form/dialog layouts reduce clutter.

No metrics dashboard, mandatory organizational setup, nesting a card around every setting, fake traffic-light controls, or decorative dots. Technical IDs/codecs/certificates are IT details rather than ordinary operator tasks.

Mac uses native SwiftUI/AppKit patterns. Windows uses WinUI 3 patterns. Shared semantics do not require copied platform chrome. Accessibility, small-window overflow, localization, focus preservation, scaling, dark/light appearance, contrast, transparency, and reduced motion need actual native verification.

D28 selects the supplied [HS Manager App icon.icon](<../assets/brand/HS Manager App icon.icon/icon.json>) for the Mac manager. Preserve its two SVG layers and separate raster grid/appearance group. The original icon remains assigned to the monitored-computer app and planned Windows resources. [Branding integration](ui.md#hs-identity-and-supplied-assets) and V51 require real target compilation, Finder/Dock/About, both Mac architectures, and supported older-OS fallback; copying the source is not native integration.

## 5. Architecture

| Component | Owns | Must not own |
| --- | --- | --- |
| Native manager | UI, accessibility, intent, platform rendering/pickers | Final authorization or arbitrary privileged execution |
| Native endpoint status | Visible active identities, permissions, help, files/messages | Private deployment keys or administration |
| Rust core | Engine adapters, typed protocol, media/input lifecycles, queues | Unrestricted public upstream remote listener |
| Desktop worker | Current actual user's capture/input/presentation/attention | General system administration |
| Privileged helper/service | Small typed machine/setup/recovery operations | Media decoding, file preview, shell strings |
| Coordination server | Local accounts, enrollment, computer access, policy, action metadata | Mandatory screen archive or decoding all previews |
| Optional relay | Admitted bounded encrypted forwarding/fan-out | Independent authority or broad host configuration |

Proposal: small Rust server with SQLite. Direct endpoint media where permitted; optional organization relay for isolated network paths. Exact transport, grant format, replay rules, signing/TLS libraries, and compatibility negotiation are reviewed proof outputs. No custom cipher or identity provider.

### macOS Computer app and endpoint

The Computer status app uses SwiftUI with AppKit/Metal/native adapters where justified. The management interface is the shared Qt/SARibbon Manager. Signed user-context capture/status processes must match the actual TCC/Accessibility permissions. Evaluate engine paths and ScreenCaptureKit adapters. PPPC/MDM does not justify a blanket promise of silent screen/input approval. Test Intel/Apple Silicon, logout/switch/sleep, missing/revoked permissions, signed updates, and capture-process identity.

### Windows Computer app and endpoint

The Computer status app uses WinUI 3 alongside a user-session capture/input worker and narrowly privileged Windows service. The management interface is the shared Qt/SARibbon Manager. Session 0 service cannot be treated as an interactive desktop. Secure IPC verifies caller/session/code identity; user-writable binaries cannot replace privileged executables. Protected/UAC desktop control is unsupported until proven safe. SendInput integrity limitations remain explicit.

### Native ABI

Freeze real C headers generated/implemented by the engine proof. Opaque handles, fixed-width values, allocator ownership, asynchronous cancellation/completion, callback threading, generation guards, frame dimensions/stride/length bounds, panic/exception handling, and version negotiation are required. Swift/C# callers cannot guess Rust layout or free Rust buffers with native allocators.

GPU surfaces/fences/copy lifetime need measurement. CPU handoff can prove correctness before optimization without pretending it passes performance targets.

## 6. RustDesk adoption gate

RustDesk is a candidate. Its current application UI is not the requested native UI, and library build targets do not prove a supported embeddable SDK. The [research ledger](research-2026-10-09.md) records exact evidence limits.

The detailed [RustDesk integration plan](spec/remote-desktop.md) defines the native handler/bridge, authenticated connection/admission sequence, receiver isolation, platform matrix, and RD01–RD12 work packages. Start with private OSS rendezvous and the coordinated secure path in an isolated feasibility fixture; reject upstream insecure continuations and enforce HS admission. Compare a product-authenticated direct adapter before freezing shipped routing. Neither raw-IP access nor a private ID server alone satisfies our authority contract. RustDesk Pro is not a dependency.

Pass/fail outputs:

1. Pin an upstream commit and audit source/build/license inventory, including media dependencies and redistribution.
2. Produce functioning Swift and WinUI native viewers without a required Flutter/Sciter product UI.
3. Document actual ABI exports, ownership, threads, disposal, errors, and maintenance boundary.
4. Test manager/endpoint combinations and both Mac architectures on real devices.
5. Enforce endpoint grant admission at every path; no upstream-password/public-relay bypass.
6. Disable unused dangerous engine capabilities rather than hide their buttons.
7. Measure 30/60-preview capture/encode/decode/network budgets and bounded scheduling.
8. Demonstrate efficient source encoding and receiver fan-out.
9. Verify session change, permission loss, expiry, crash, and unique-controller teardown.
10. Record upstream update/rebase/patch ownership and an honest fallback if adoption fails.

An E01 failure changes the remote-engine decision rather than weakening native UI/security requirements. Do not build an extensive production UI around a fictitious engine API.

## 7. Accounts, inventory, configuration

Local named organization accounts first. Administrator MFA and recovery are initial security requirements; operator MFA is deployment-configurable, stronger for later remote exposure. Maintained implementations enforce hashing/factors/rate limits/recovery/session expiry. Ordinary server/service credentials are not operator identities.

Administrative capabilities have independent account/computer scopes and delegation ceilings. Routine install/enrollment/account maintenance does not implicitly grant operation rights. Account reset produces recovery-only verification, not a normal target login chosen by IT. Deliberately trusted security authority may make explicit reviewed grant/self changes with fresh MFA; its power and the host-administrator trust limit are stated honestly. First-manager approval has an exact single-use local/tunnel bootstrap.

Access assigns capabilities to computer IDs. Group selection in access editing stages explicit IDs; later group edits do not silently grant access. Optional personal/shared group editing stays distinct from authorization. Direct per-computer effective permissions show their source.

Enrollment profiles have actual trust identity, unique-device key binding, expiry/uses, administrator approval, and atomic consumption. Management computers require one-time administrator approval, checked alongside ordinary account sign-in. Revoked/replaced manager identities need renewed approval. Images create identities after cloning. Public connection exports exclude private deployment keys. Discovery never grants enrollment or access.

Configuration layers: product-safe defaults, organization policy, explicit computer override where permitted, then user UI preferences. Permissions combine restrictively. Versioned schema validation occurs before transactional save. Saved, Delivered, and Applied revisions remain distinct. Secret storage uses selected OS/server key stores.

CSV/config import previews changes before apply. External OIDC/LDAP/AD integration is later and must preserve local recovery. There is no required room/class directory.

## 8. GUI self-hosting and noninterference

One guided assistant in the shared Manager offers hosting routes according to the build's host platform. The Windows build offers hosting on this Windows computer or a supported Linux installation over SSH. The macOS build offers This Mac hosting through a product-owned launchd service or a supported Linux installation over SSH. Remote Windows installation remains unsupported. The local hosting routes remain subject to installer, readiness, ownership, update, and recovery evidence. The routes join at host checks and the shared network, identity, administrator, storage, review, and install steps. The server repo owns installation backend/resource manifests; the Manager owns the shared wizard. Optional containers are an advanced alternative, not mandatory Docker.

The assistant checks host/privileges/disk/path/port conflicts on the selected route, configures intended LAN scope, creates deployment identity and first local administrator/MFA, sets storage/backup, reviews exact changes, and installs with a phase journal. First-time setup finishes at Server Ready for Enrollment: the installed service is healthy and enrollment can accept a profile. Deployment Verified is separate later evidence that one real enrolled authorized endpoint produced authorized media, denial, teardown, and recovery. A TCP probe alone cannot mark Deployment Verified.

Bootstrap administration is local-only or through authenticated SSH tunnel, never an unauthenticated LAN setup form. On the Linux SSH route, verify host identity before any write. Cancel before Install changes nothing; later cancellation/rollback follows actual phase/migration boundaries.

Use dedicated service names/identities, product paths, permissions, bounded logs, CPU/memory limits where supported, quotas, and an exact ownership manifest. Do not replace system runtimes, rewrite a global firewall, overwrite another RustDesk install, delete unrelated services, or require occupied 80/443. Offer free alternative configured product ports with clear scope. Remove keeps data/backups by default.

RustDesk listener numbers in the research ledger describe its upstream server, not final product ports. Freeze chosen listeners after the engine/network proof, with unused WebSocket/public fallback closed. Do not expose all upstream examples automatically.

Backups are consistent, encrypted, bounded, and tested. Restore checks schema/keys/integrity, invalidates live authorization generation, and reconciles post-backup identity revocation. Clean-host continuity and compromise-key replacement are distinct procedures. Recovery material is stored separately from the host.

There is one active deployment authority in version 1. Moving/restoring it requires old-host fencing, independent fresh-issuer approval, and per-peer persisted trust adoption. An offline peer is Pending Trust Update rather than silently migrated; inaccessible identical-secret server copies cannot be universally detected/fenced. Required persistence failure blocks new grants/effects and offers scoped storage recovery, while safety stop/release stays available.

Updates use signed compatibility manifests and offline imports. Pilot/drain staged upgrades; report offline endpoints and pause failures. Rollback requires proven code/data compatibility. Repair/removal touch only product-owned resources.

## 9. Security and privacy

Threats include a malicious managed user, stolen operator credentials, LAN attacker, wrong computer/desktop identity, compromised manager, hostile files/media, installer misuse, resource exhaustion, and update compromise.

Trust is bound to deployment, account, manager key, target key, desktop, operation, interaction/action, policy revision, lifetime, and replay context. Server and endpoint/helper validate independently. UI role hiding is not authorization. IT stop/recovery is separate from observe/control permissions.

Bound all parse/allocation/admission/queue/storage boundaries. Validate typed protocol/unknown enums, URLs, catalog args, frame geometry, chunks, filenames, archives, paths/symlinks/reparse points, IPC caller identity, and quota races. Use timeouts/bounded retries with side-effect-specific semantics. No generic reconnect retry of destructive work.

Default privacy: visible observation/control, no microphone/camera/keylogging, no continuous screen archive, no credential/message/clipboard content in logs. Screenshot and file content have distinct explicit storage/export rights. A proposed audit-metadata retention is 30 days, configurable; server file retention/quotas need measured numeric limits and administrator policy. This is not a legal retention requirement or compliance certification.

Publish corresponding source/build information/license notices for actual adopted obligations. Exact SPDX/dependency/codec redistribution and signing pipeline need the release audit; AGPL-compatible intent alone is insufficient.

## 10. Code quality and verification

Use small modules with one ownership boundary, generated typed contracts, explicit concurrency/cancellation, no exception/panic across ABI, and platform UI isolation. Stable reason codes localize human copy. Keep account/identity secrets out of ordinary logs/support exports.

Each owning repo runs its real format/lint/build/meaningful boundary tests. Rust tests include protocol/authority/ownership/resource bounds; native tests include interaction state/main-thread/disposal; server tests include access scope/enrollment/action journals/migrations/restore. Real capture/input/TCC/service/installer/performance claims need actual device evidence, not fixtures.

[Verification scenarios](spec/verification.md) define pass/fail and retained evidence. [Build map](spec/build-map.md) stages proofs and vertical slices. No source commands/toolchain version are invented before the implementation repositories and pinned dependencies exist.

## 11. Optimization targets

Unmeasured draft budgets; freeze reference hardware, versions, load, and measurement definitions through E06.

| Workload | Initial target |
| --- | --- |
| Idle endpoint | At most 0.02 CPU core average, 150 MiB memory, no inactive screen capture |
| 30 simultaneous previews | Approximately 320×180 at 1 fps, p95 receipt freshness within 2 seconds |
| Preview network | At most 256 kbit/s average per target under reference workload |
| Manager at 30 previews | At most 750 MiB; responsive native selection/search/stop actions |
| Assistance wired LAN | 1080p30 target, p95 input-to-visible response within 150 ms, start within 3 seconds |
| Server, 300 enrolled computers | At most 512 MiB excluding relay/file buffers under defined metadata load |
| Long running | Eight-hour run without monotonic memory/handle growth |

Evaluate 60 previews and weak Intel/Windows hardware separately. Large inventory does not mean decoding every screen continuously. Visible demand, multiple displays, content motion, codec availability, hardware decode limits, relay fan-out, and file traffic alter cost. Optimization cannot bypass admission/disclosure or make stale frames look live.

## 12. Repositories and releases

| Implementation area | Owns |
| --- | --- |
| apps/manager | Shared C++20/Qt Widgets/SARibbon Manager for Windows and macOS; independently built and currently integrated in main |
| apps/macos | Swift Computer status app, native rendering/OS integration, capture/input worker and helper |
| apps/windows | WinUI 3 Computer status app, Windows worker/service/package integration |
| modules/core | Shared Rust contracts/engine adapters and generated native artifacts |
| modules/server | Coordination service, migrations, installation backend/manifests, relay integration |

main owns planning, ADRs, integration verification, and exact tested gitlinks/compatibility manifest. Implementation belongs in its own repo and must build without sibling-relative dependencies. Shared releases use pinned source or versioned signed artifacts. A dedicated upstream engine fork repo is optional only if maintenance evidence requires it.

A release manifest identifies source commits, binary hashes/signatures, license/source links, architectures, ABI/protocol/schema compatibility, security revision/release sequence, trusted policy/metadata, tested platform capabilities, and upgrade/authorized rollback evidence. A submodule bump is a reviewable integration change, not an automatic latest-branch checkout.

Four independently buildable implementation repositories are now checked out as pinned submodules. BUILDING.md and the implementation evidence record the development foundation; these do not establish production media/control or installation readiness.

## 13. Delivery gates

| Stage | Required outcome |
| --- | --- |
| Foundation | Product answers, package identity, real contract schema/limits, threat/ownership model |
| Engine/platform proofs | Actual native bridge, endpoint permission/input/process behavior, preview/fan-out measurements |
| First vertical slice | Guided server, local account, enrolled Windows endpoint, Mac automatic preview, explicit control/stop, denial/crash recovery |
| Monitoring pilot | Both managers/endpoints, optional groups, 30/60 previews, sharing/attention/messages/help, partial failures |
| Full version 1 | Files/requests/apps/screenshots/power, fleet updates, native accessibility, occupied-host installation, restore/security assessment |
| Advanced modules | Dedicated permission/enforcement/retention/recovery proof for each later module |

The pilot is not the finished version 1. The user confirmed Helicoid (Systems), the D18 inventory-scope draft, and D21 request closure retaining completed files with explicit replacement confirmation on 10 October 2026. Durable release package/service identities remain to be frozen. Server storage, multiple viewers/one controller, manager approval, the new Mac manager icon D28, queued files waiting for their intended user D29, and presentation stop only by the sharing owner/authorized IT D30 are confirmed. Technical facts require proof rather than asking the user to select arbitrary codec/TLS/IPC constants.

## 14. Evidence boundary

Existing primary-source research is in [research-2026-10-09.md](research-2026-10-09.md). Repository/local documentation checks can prove consistency, not product runtime behavior.

Not verified: native builds, RustDesk embedding, actual authority/cryptographic integration, installer/MDM outcomes, performance, enforcement, live hosting, signed updates, or restore. The initial planning pass created no implementation. Subsequent development evidence is recorded separately in implementation-2026-10-10.md. The current wave includes public-network deployment using organization-owned rendezvous/relay, explicit enrollment trust and non-bypassable endpoint admission; no successful production deployment is claimed here.
