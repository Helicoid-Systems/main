# Screen and interaction specification

Source specification updated on 19 October 2026 for the accepted 74-family native redesign. This document is the planning source of truth for layout, actions, unavailable and failure behavior, and focus. It is separate from delivered runtime: no native implementation, pixel, permission, capture, or installation claim is made here. [System contracts](spec/contracts.md) govern authority and lifecycle; [workflows](spec/workflows.md) govern dispatch, failure, and cancellation. The main screen is Computers. Account permissions apply directly to computers.

> **Manager presentation update, 20 October 2026:** the shared Qt/SARibbon
> direction in [ADR 0008](adr/0008-shared-qt-manager.md) supersedes the
> framework, window chrome, and navigation choices in this native UI draft.
> Retain the screen behavior, authority, accessibility, and failure rules as
> migration requirements while the Qt specification is implemented.

Every screen family identifier C01–C03, M01–M25, E01–E11, A01–A11, H01–H19, and P01–P05 is preserved. The acceptance ledger at [evidence/ui-acceptance-2026-10-19.json](evidence/ui-acceptance-2026-10-19.json) records each identifier as Not Verified until real source, build, and runtime evidence exists. Documentation never marks a capability Verified.

## Design contract

Simple, understandable UI with minimal clutter, guided by Apple's Human Interface Guidelines and Windows command-bar and focus guidance. App-written text has no dot separators, decorative dot glyphs, bullet glyphs, or loading ellipses. Use spacing, separate lines, and plain labels. Ordinary sentence, address, version, and file punctuation remains useful. System permission dialogs retain their system wording.

Computer names are primary. A trusted current user name is optional secondary information. Do not require a student account, seat, room, class, timetable, or course. Use Operator in administration and the person's name in user-facing interaction messages.

| Guidance | Application | Primary source |
| --- | --- | --- |
| Hierarchy and progressive disclosure | Computer screens dominate; connection and technical details open on demand | [Layout](https://developer.apple.com/design/human-interface-guidelines/layout), [Disclosure controls](https://developer.apple.com/design/human-interface-guidelines/disclosure-controls) |
| Selective toolbars | Monitoring, Share Screen, Message, and Actions; sensitive stop controls appear while needed | [Toolbars](https://developer.apple.com/design/human-interface-guidelines/toolbars) |
| Descriptive writing | Pause Monitoring, View Screen, Take Control, Stop Control | [Writing](https://developer.apple.com/design/human-interface-guidelines/writing), [Buttons](https://developer.apple.com/design/human-interface-guidelines/buttons) |
| Purposeful interruption | Routine results stay in context; destructive actions and identity changes have specific reviews | [Alerts](https://developer.apple.com/design/human-interface-guidelines/alerts) |
| Accessible meaning | Text accompanies selection, help, failure, and live-control state | [Accessibility](https://developer.apple.com/design/human-interface-guidelines/accessibility) |
| Native appearance | System typography and accent; dependable contrast around busy remote screens | [Materials](https://developer.apple.com/design/human-interface-guidelines/materials), [Color](https://developer.apple.com/design/human-interface-guidelines/color) |

These principles were checked against official indexed guidance on 9 October 2026 and refined on 19 October 2026 against the slop-research ledgers. Direct Apple pages returned JavaScript shells. Their use here is design interpretation, not Apple approval.

### Shared presentation rules

These rules bind every screen below. They come from the accepted refinements and the community and platform slop research dated 19 October 2026.

- Refined native system typography, accent, and window chrome. Supplied artwork is unchanged.
- Purposeful SF Symbols and Fluent icons in the 16 to 20 point range. Modest headings. Neutral content surfaces. Spacing uses 8, 16, and 24 point steps.
- One meaningful group or container per related set of controls. No nested cards, decorative pills, healthy-status badges, marketing headings, developer copy such as Dispatch, targets frozen, Nothing invented, or Preview requires engine, and no repeated policy, company, or role text.
- Keep essential operator, permission, trust, and release meaning. Concise copy must not conceal active observation, control, permission purpose, pending release, or cleanup guarantees.
- Bottom actions stay reachable while the body scrolls. Small work areas scroll or overflow rather than clip.
- No loading ellipses. Honest measured progress uses determinate advancement when work is measured and an indeterminate spinner when it is not.
- Focus first input. Destructive default is neutral or cancel. Inline errors beside the failing field or row. Safe draft retained after failure. Secrets cleared when a flow ends or identity changes. Honest measured progress only; no fabricated completion.
- Native window transitions only. Remove custom scale animations and decorative timers.
- Accessible in light, dark, inactive, high-contrast, reduced-motion, and increased-text-scale conditions. Labels accompany color and icon state.
- Native keyboard commands and context menus. No canvas focus trap. Reserved host OS and application shortcuts stay local.

### Logical window sizes

| Window | Default | Minimum or note |
| --- | --- | --- |
| Connect and Sign In | 520 × 360 | Adaptive to content and text scale |
| Verify Server | 600 × 440 | |
| Setup | 600 × 400 | |
| Computer permissions | 600 × 300 | Top-aligned content |
| Endpoint status | 420 × 260 | |
| Computers | 1120 × 760 | Minimum 720 × 480 |
| Remote | 960 × 640 | Minimum 560 × 360 |
| Administration | 1040 × 720 | |
| Composer | 560 wide | Height follows content |
| Results | 720 × 460 | |
| Settings | 640 × 440 | |
| About | 360 × 340 | |

Center a compact window once on first open. Do not re-center on every revisit. Small work areas use scroll and overflow.

### Preview, streaming, and input

- Preview tile size Compact 192, Standard 256, Large 320 default, ExtraLarge 448. Remember the choice per real account and deployment, not per fixture. Columns reflow at the chosen density; do not auto-shrink density to fit. List view shows no continuous thumbnail stream. Computer names stay readable at every size.
- Reachability, availability, observation authority, and control ownership are separate states. An online host never implies a live stream.
- Show Updates Delayed after 3 seconds without a valid stream health signal; clear retained pixels after 5 seconds without valid health. A static desktop is not stale solely because no new pixels arrived. Fresh valid health removes the delayed state, but pixels require a current admitted frame. Clear pixels immediately on confirmed disconnect, authorization loss, desktop or display invalidation, or earlier lease expiry. These presentation thresholds require engine proof before enablement; they do not extend authority.
- Stop Control uses Command-Shift-Escape on Mac and Control-Shift-F12 on Windows. Control-Tab exits the remote canvas locally. Reserved host OS and application shortcuts remain local. Only focused canvas input is remote. The first focus interaction is consumed locally and not forwarded.
- Menu, dialog, or focus loss cleans held input immediately. No queued input is replayed after reconnect, restore, or takeover.
- The release registry survives viewer close. Pending Release remains visible until endpoint acknowledgment or a safe expiry bound, never an arbitrary local timer.

## HS identity and supplied assets

On 9 October 2026, the user supplied the original HS assets and confirmed that HS represents both the company and product. On 10 October 2026, the user selected a separate attached icon for the Mac manager. Original files are preserved in main; [manifest.json](../assets/brand/manifest.json) records their provenance, platform/role assignments, byte counts, and SHA-256 hashes. This identity does not settle the full display name, package identifiers, or artwork distribution metadata.

| Asset | Source | Planned use |
| --- | --- | --- |
| Mac manager app icon | [HS Manager App icon.icon](<../assets/brand/HS Manager App icon.icon/icon.json>) with its two SVG layers and PNG grid layer | The selected Mac manager icon. Preserve both groups, positions, opacity, materials, and light/dark/tinted appearance settings exactly; retain in the Mac Manager target and verify the packaged result |
| Original app icon | [HS App icon.icon](<../assets/brand/HS App icon.icon/icon.json>) with its two SVG layers | Retain for the Mac monitored-computer app and planned Windows package/window resources. The Mac manager assignment uses the separate icon above |
| Dark-lettering logo | [hs logo.svg](<../assets/brand/hs logo.svg>) | Logo on light surfaces |
| White-lettering logo | [hs logo white.svg](<../assets/brand/hs logo white.svg>) | Logo on dark surfaces |

These are the supplied originals, not newly generated or recolored artwork. Both logo files have the same square 2000 by 2000 view box and colored stripe. Choose the matching lettering variant for the actual surface and preserve the view-box aspect ratio. Keep the original stripe colors within the artwork; interface controls continue to use native system accents.

The new manager icon is a four-file bundle: icon.json, two referenced SVGs, and gridmakerpro-grid-only-300dpi-2026-10-10.png. The matching SVG bytes are retained independently in each original bundle; do not replace the new icon with the old two-layer composition or omit the raster grid. This user instruction changes the Mac manager icon assignment only. Company logos, endpoint identity, package identifiers, and Windows artwork assignments follow their existing contracts.

Connection C01–C03 uses one small HS mark above the task heading, with the verified deployment name and form below. It is decorative alongside the visible identity text; assistive technology must not announce the same name twice. Enrollment E01–E03 and first server setup H01 use the same restrained treatment. About P04 shows the app icon, approved display name, version, company identity, source, and acknowledgments. Other setup pages use ordinary task headings.

Computers M01–M04 keeps its existing title, toolbar, and preview grid. Remote M06–M07 preserves space for pixels and Stop Control. Endpoint status E04–E07 prioritizes operator identity and current access. Status, menu-bar, and tray symbols need legible native-size resources; the full logo is not assumed to work at those sizes. Branding never replaces computer names, connection status, or security indicators.

Each application repository bundles its required approved assets or generated resources, with the source manifest revision recorded. Independent builds must not read an adjacent main checkout at runtime or build time. Main remains the canonical source for supplied artwork; asset changes and generated platform resources are reviewed together when pins change. The Mac manager target must select the new manager icon explicitly and About P04 must use that target's compiled icon. Verify Finder, Dock, About, supported appearances and scales, and fallback on every supported Mac OS and architecture; no deployment-target increase is authorized by an icon change. If the chosen compiler needs older-OS fallback resources, derive them from this same approved composition and verify them through the frozen toolchain rather than recoloring or substituting artwork. V51 owns this proof. Application repositories contain platform resources and provenance checks; packaging checks do not establish Finder, Dock, appearance, or older-OS runtime acceptance. Record those results against each actual artifact.

Primary source checked on 10 October 2026: [Apple Icon Composer](https://developer.apple.com/icon-composer/) describes layered icon files integrated with Xcode. The supplied bundle's format and hashes are observed locally; successful compilation and older-OS behavior still require the real Mac project and selected toolchain.

## Main manager window

The default is one flat computer grid. A compact, collapsible Computers panel chooses monitoring inclusion through All Computers, optional flat group checkboxes, and individual computer checkboxes. All Computers works immediately without creating a group. Several groups can be included at once. Groups have no courses, schedules, or required hierarchy; duplicate membership never duplicates a tile.

The title is Computers. Search is in window chrome. A native Grid and List view control and preview-size command are available without a settings tour. An optional compact status filter can show All, Online, or Needs Attention. No permanent metrics row or healthy-status badge on every computer.

Live previews begin automatically after the authorized console opens and per-computer admission succeeds. The normal toolbar is Share Screen, Message, and Actions. Pause Monitoring stops this console's previews and related control; Resume Monitoring obtains fresh admission. There is no start or setup step between signing in and seeing authorized screens.

Each tile contains a 16:9 or source-preserving letterboxed preview, stable computer name, optional trusted user label, and exceptional status only. Before observation admission it contains a readiness placeholder. A single click selects; double-click or Open View Screen opens view-only. Standard additive and range selection has a keyboard equivalent. Reconnection does not reorder tiles or clear selection.

With a selection, the scope line says 3 Computers Selected and offers Clear Selection. Without selection, it says All 6 Displayed Computers, or No Computers Displayed. Search and status filters restrict the default scope of a new action. Changing inclusion, search, or status clears temporary tile selection; an already opened action retains its frozen IDs. Every composer shows the exact target names and count before dispatch. Permissions and action eligibility are checked independently of those display choices.

Administration opens separately through an authorized menu command. Settings, About, Help, and Quit use platform locations. Help requests are a compact contextual filter or window, not a teaching sidebar. Optional groups cannot become an extra required navigation layer.

### Veyon workflow adapted to the native apps

This is the native adaptation of confirmed D22. The [source ledger](research-2026-10-09.md#veyon-source-review-computer-management) distinguishes inspected Veyon behavior from these design choices.

| Element | Exact proposed behavior |
| --- | --- |
| First opening | Resolve current account access on an approved manager; include all authorized inventory by default; begin admitted previews automatically |
| Computers panel | Show or hide with a labeled toolbar or menu command. Persist visibility per account and deployment; hide initially when no groups exist. The command remains available for individual inclusion |
| All Computers checkbox | Checked when every current authorized inventory ID is included; mixed when some are included. Checking selects All mode; unchecking selects an explicit empty set |
| Group checkbox | Calculate checked, mixed, and unchecked from current member IDs that the account can see. Checking adds all those IDs; unchecking removes them. Clicking mixed adds all. Overlapping group states update from the resulting ID set |
| Individual computers | A disclosure below each group, plus Ungrouped Computers where needed, shows name and include checkbox. Removing the last membership does not delete a computer or its access |
| Stored inclusion | Store All mode or explicit included IDs, separately from authorization and temporary action selection. Restore only against current deployment and account inventory. In All mode, eligible new inventory can receive fresh admission under D18; explicit mode never silently adds IDs |
| Checkbox changes | Change overview demand and clear temporary tile selection. Removing a computer stops its overview stream and clears its tile image. Independent detailed windows and fixed sharing or file recipients are not retargeted |
| Single click | Select one displayed tile. Platform modifier adds or removes; Shift extends along the current displayed stable sort order. Keyboard selection has equivalent behavior |
| Search or status change | Apply to included IDs; clear temporary selection; preserve stable sort. Search computer name and trusted current user name. Filters never erase enrollment or grant permissions |
| No selected tiles | New command targets exactly the displayed filtered IDs, including unavailable IDs still displayed. Zero displayed IDs disables commands requiring targets |
| Selected tiles | New command targets only selected displayed IDs. Select All selects the displayed set. Clicking empty grid space or Clear Selection clears the temporary selection |
| Action opened | Freeze IDs, display exact names and count, then show support, permission, or offline exclusions. Revocation can deny execution but cannot replace targets. Partial results refer to this snapshot |
| Context menu | Right-clicking an unselected tile selects only it; right-clicking a selected tile keeps the selection. Show the same authorized commands as the toolbar. Keyboard context-menu command follows the focused tile or selection |
| Double-click or Open | Open that one computer in a separate view-only window. No control or broad action occurs |
| Preview sizing | A visible Preview Size command and the native View menu offer Compact 192, Standard 256, Large 320, and Extra Large 448. Remember per account and deployment. Resize tiles without changing scope, sort, selection, or stream authority; honor source aspect ratio |
| Overview display | Show the endpoint's current primary display. Do not combine all displays. Primary or layout changes refresh authorized source identity; an unavailable primary display shows readiness rather than silently choosing another |
| Active modes | Keep Share Screen and Message in the normal toolbar, other commands in Actions. Show Stop Sharing, Release Attention, and Stop Control while applicable. Review replacement of this operator's conflicting mode on exact affected targets; another owner's mode produces a conflict |

In native code, panel inclusion, displayed IDs, tile selection, action snapshots, and actual admitted streams are separate state objects. Do not reuse one boolean or selected-ID array for all five. The web study uses fictional computers and results; it is not capture, policy, or security proof.

## Remote window

Remote pixels dominate. The title says Viewing PC-01 or Controlling PC-01. Take Control is explicit. Stop Control remains visible during assistance. Display picker appears only for multiple displays. Fit and Actual Size use familiar native controls. Technical connection details are collapsed.

Selecting a tile never starts control. Closing the remote window ends its interactions. Disconnect, permission loss, or desktop change replaces remote pixels with a named state and releases input. Reconnect can reopen authorized view-only; it cannot silently resume control.

Minimizing releases control while authorized viewing stays active. It also cancels pending Take Control. Stop forwarding and release held input immediately; keep Pending Release visible after restore when endpoint release is unresolved. Restoring shows Viewing and requires a new explicit Take Control after release reconciliation. Focus alone cannot restore control. Other operators remain independent.

## Monitored-computer interface

A compact menu-bar or tray status window shows server connection and active operator identities. Use Your Screen Is Being Viewed or Morgan Is Controlling This Computer. A user can Request Help and view messages and files where enabled. The managed control contract has no user Stop Control command; operator or authorized IT can stop, and security teardown still applies automatically.

Observation and control indicators follow actual admitted activity. Multiple authorized observers are listed with names; exactly one controlling operator is identified. Attention and sharing surfaces identify their owner and tested recovery behavior. No Waiting for a Lesson state.

## Native composition and behavior

Use system typography and ordinary title, body, and secondary hierarchy. Use system accent for selection and actions and neutral content surfaces. Healthy states remain quiet. Avoid nesting a card around each form or status. Keep one main action per short dialog and put actual consequences before destructive confirmation.

The Manager uses shared C++20, Qt Widgets, and SARibbon on Windows and macOS. Use each platform's native window controls, file dialogs, clipboard, shortcuts, and accessibility conventions. The monitored-computer status apps remain platform-specific: SwiftUI/AppKit on macOS and WinUI 3 on Windows. Apple's hierarchy and restraint guide content clarity without replacing the Manager's shared Office 2021 ribbon shell.

Use short interruptible transitions without bounce or decorative looping motion. Preserve keyboard focus, scroll, and selection during updates. Respect reduced motion, contrast, transparency, text scaling, appearance, and notification settings. Do not announce every frame through assistive technology.

A small laptop still needs readable computer labels and reachable stop actions. Toolbar commands overflow into labeled native menus before clipping. Preview sizing can reduce columns without shrinking names to unreadable text. Detailed results and settings can scroll; essential scope and primary and stop actions stay reachable. Actual window sizes, control metrics, and accessibility must be verified natively rather than inferred from the web study.

## Screen inventory

These are screen families. Reuse native components rather than create one bespoke controller per row. All 74 families are specified below. Implementation phases order the work; specification status is not runtime status.

Each family records four aspects: Layout, Actions, Unavailable and failure, and Focus. Unavailable and failure covers the states that must be honest rather than hidden. Focus covers keyboard entry, tab order, and what is consumed locally.

### Connection

#### C01 Connect to Server

- **Layout.** One small HS mark above the task heading. Verified deployment name and address or profile field below. Trusted connections appear as a short list of rows with name and source. No development identity fields. Window 520 × 360 adaptive.
- **Actions.** Connect. Import a verified profile. Open Set Up Server. Select a trusted row to prefill.
- **Unavailable and failure.** Invalid address stays inline beside its field. Discovery never establishes trust. An unreachable server keeps the typed address and offers Retry and a contact route. Import of an unverified or malformed artifact refuses before any trust change.
- **Focus.** Focus starts in the address or profile field. Tab moves to trusted list, then Connect, then Set Up Server. Return activates Connect when the field is valid.

#### C02 Verify Server

- **Layout.** Deployment name, actual verification identity, and a short trusted-source explanation. One meaningful identity group. Window 600 × 440. The HS mark is decorative beside the visible name and is not announced twice.
- **Actions.** Trust Server after verification. Cancel returns to C01 without persisting trust.
- **Unavailable and failure.** Changed identity stops access and shows a blocking review with the exact old and new identity. There is no trust-anyway action. Verification failure keeps the previous trusted state untouched and offers Retry or Contact IT.
- **Focus.** Focus starts on the primary Trust Server action only after verification text is populated; otherwise focus stays on the verification status row so the user must read it. Cancel is reachable and is the default destructive-safe action.

#### C03 Sign In

- **Layout.** Native local-account fields with the verified server identity above. Conditional MFA step follows the password step. Enrollment offers QR, manual entry, or recovery codes. Pending manager state replaces the form when approval is outstanding. Window 520 × 360 adaptive.
- **Actions.** Sign In. Next for MFA. Check Approval and Contact IT while pending. Enroll via QR, manual, or recovery codes. Session-expired and recovery-only subflows replace the ordinary form with their own bounded fields.
- **Unavailable and failure.** Actual rate-limit expiry is shown as remaining time, not a generic retry. Factor failure retains safe non-secret context. Pending approval shows no pixels. Recovery-only credentials cannot sign in normally and state that limit plainly. Secrets clear when the flow ends, the session expires, or identity changes. Safe draft of non-secret fields is retained after failure.
- **Focus.** Focus starts in the first empty credential field. MFA moves focus to the code field. Destructive or sign-out confirmation defaults to Cancel. Enrollment QR uses a native capture surface with a visible cancel; no canvas focus trap.

### Manager

#### M01 Computers Connecting

- **Layout.** Inventory with stable names and readiness placeholders, search in chrome, Grid and List control, optional inclusion panel. Title Computers. No metrics row. Window 1120 × 760, minimum 720 × 480.
- **Actions.** Automatic admission after authenticated opening. Search, filter, switch Grid and List, open the inclusion panel, open Administration and Settings from platform menus. No Load Inventory prerequisite.
- **Unavailable and failure.** Unavailable targets do not block the rest. Failed inventory load shows a named recovery beside the affected region with Retry; it does not fabricate tiles. Connection loss shows Reconnecting to Server and keeps last known names without stale pixels.
- **Focus.** Focus starts in search. Tab order is search, view control, inclusion panel, then the grid. First grid interaction is consumed locally.

#### M02 Monitoring Paused

- **Layout.** Same inventory with readiness placeholders instead of pixels. Scope line still shows displayed count. Unresolved pending-release link appears only when needed.
- **Actions.** Resume Monitoring. Inclusion and search remain available. Open View Screen is unavailable while paused.
- **Unavailable and failure.** Resume obtains fresh admission; failure names the failed stage and keeps the paused state. No start confirmation for ordinary viewing. Cleared pixels are not restored from cache.
- **Focus.** Focus lands on Resume Monitoring. Search and inclusion remain tab-reachable. Pending-release link, when present, is a separate focus stop.

#### M03 Monitoring

- **Layout.** Live grid or list, restrained toolbar with Share Screen, Message, and Actions, scope line, and optional status filter. Tiles show preview, stable name, optional trusted user, and exceptional status only. Natural name and identifier sort is stable.
- **Actions.** View Screen, Share Screen, Message when implemented, Pause Monitoring, contextual commands on selected or displayed targets. Quiet healthy states show no badge.
- **Unavailable and failure.** Per-computer unavailable or stalled states clear unsafe pixels immediately. Distinct empty, no-inclusion, and no-results states. Share Screen and Message appear only when their modules are implemented; absent modules are not stubbed with developer copy. Unrelated observers remain independent.
- **Focus.** Keyboard route from search to selection to View Screen. Context menu command follows focused tile. Scope line Clear Selection is a focus stop. No repeated policy text in the toolbar.

#### M04 Selection

- **Layout.** Same inventory with displayed and selected counts, Clear Selection, and optional inclusion panel. One selection group in the scope line.
- **Actions.** Standard additive and range selection with keyboard equivalents. Select All selects the displayed set. Inclusion, search, or status change clears temporary tile selection. An open action retains its frozen IDs and shows exact names and count.
- **Unavailable and failure.** Zero displayed IDs disables commands that require targets. Revocation can deny execution but cannot replace a frozen target snapshot. Partial results refer to that snapshot only. Changing filters never erases enrollment or grants permission.
- **Focus.** Clear Selection is reachable without leaving the grid. Keyboard range selection follows the current displayed stable sort. Composer opens with focus on the primary action after the target list is visible.

#### M05 Help Requests

- **Layout.** Stable list of computer, user name, request age, and optional short text on detail. One list container. No teaching sidebar.
- **Actions.** View Screen or Resolve Request, independently. Optional filter to unanswered requests.
- **Unavailable and failure.** No request grants control. Deduplicate repeated taps from the same computer. Expired or withdrawn requests leave the list with an honest removed state rather than vanishing silently. Transport-required failures name the missing capability.
- **Focus.** Focus starts on the request list. Resolve Request is the primary action and never steals focus from another window. Opening View Screen does not resolve the request.

#### M06 View Screen

- **Layout.** Separate native view-only remote window per computer. Title Viewing PC-01. Conditional display picker only for multiple displays. Fit and Actual Size controls. Technical details collapsed. Window 960 × 640, minimum 560 × 360.
- **Actions.** Take Control. Switch display when offered. Fit or Actual Size. Open Details. Close ends this window's interactions.
- **Unavailable and failure.** Connecting and Unavailable replace pixels with named states. No duplicate Close or headings. Reopen focuses the existing window rather than creating a second. Permission or desktop change replaces pixels and clears stale frames immediately. No silent control resume on reconnect.
- **Focus.** Focus starts on the remote canvas only after the user interacts; the first focus interaction is consumed locally and not forwarded. Control-Tab exits canvas locally. Reserved host shortcuts stay local.

#### M07 Assistance

- **Layout.** Remote window with persistent Controlling state and Stop Control in the toolbar. Separate Input Paused hint when local UI has focus. Display picker only for multiple displays.
- **Actions.** Take Control after explicit reservation and endpoint acknowledgment. Stop Control uses Command-Shift-Escape on Mac or Control-Shift-F12 on Windows. Minimize releases and cancels control while viewing stays active. Restore requires a new Take Control.
- **Unavailable and failure.** Input Paused, competing owner, protected desktop, missing input permission, and user change each get a specific result. Pending Release remains visible until acknowledgment or safe expiry and survives viewer close. Minimize cancels a pending claim. Focus loss stops forwarding and cleans held input. No input replay. Unrelated observers stay independent. The computer user has no Stop Control command under D09.
- **Focus.** Only focused canvas input is remote. Toolbar, menu, and dialog input stay local. Menu or dialog open cleans held input. First focus interaction after canvas entry is consumed. Stop Control is always a reachable focus stop.

#### M08 Share Screen Source

- **Layout.** Native display or window picker, or a separately authorized remote source. Fixed recipient list with names. Window 560 wide composer. No default audio.
- **Actions.** Start Sharing. Choose local source or separately authorized remote source. Confirm fixed recipients. Explain operator or authorized IT stop and presentation or fullscreen behavior inline once.
- **Unavailable and failure.** Source problems and excluded recipients stay inline with exact names. No closed-window to desktop fallback. Unavailable module shows its honest capability state rather than a disabled developer dialog. Changing source is explicit and reviewed.
- **Focus.** Focus starts on the source picker. Recipients follow. Start Sharing is the single main action and is disabled until source and recipients are valid. Cancel is the default safe action.

#### M09 Sharing

- **Layout.** Compact source and recipient strip in the manager window. Per-recipient branch rows with actual states: Receiving on Screen, Minimized, Stopped by Operator or IT, Unavailable, Stopped, and Unknown.
- **Actions.** Stop Sharing for the whole distribution. Per-branch stop for one recipient. Change source explicitly. Open Details for consequences.
- **Unavailable and failure.** Source stop ends all branches with stated consequences. Zero receivers ends distribution. No resurrection after stop, source loss, or expiry. Unknown state never implies Receiving. Unavailable recipients keep their row until the branch result is known.
- **Focus.** Focus stays on the strip when a branch changes. Per-branch stop and whole-distribution stop are distinct focus stops with distinct labels. No focus theft from other windows.

#### M10 Attention

- **Layout.** Short bounded message field and exact target list. Active state shows owner and Release Attention. One composer container.
- **Actions.** Request Attention. Release Attention. Review conflicts before replacement.
- **Unavailable and failure.** Conflicts review names affected owners and recipients and waits for teardown before replacement. Show apply and release acknowledgments. Actual platform restriction is labeled separately from any OS lock. Watchdog recovery is available to authorized IT. Expiry is shown as remaining time.
- **Focus.** Focus starts in the message field. Request Attention is the main action. Release Attention is a separate primary while active and is never buried. Destructive cancel defaults to neutral.

#### M11 Message

- **Layout.** Plain-text composer with exact recipient names and count. Window 560 wide. No formatting chrome beyond plain text.
- **Actions.** Send Message. Per-target delivery results after dispatch.
- **Unavailable and failure.** Empty and oversized input validates inline at 1000 Unicode scalars maximum. Retain failed draft. Show actual delivery results per target. No read receipt is inferred. Recipients without transport show as unavailable rather than silently dropped.
- **Focus.** Focus starts in the text field. Send Message is the main action and becomes primary only when input is valid. Draft retention does not steal focus on failure; the inline error is the focus target.

#### M12 Open Website or App

- **Layout.** One field accepting a valid HTTP(S) URL, or a selection from the IT-approved typed application catalog. No shell or privileged path field. One action group.
- **Actions.** Open Website for a URL. Open App for a catalog entry. Show launch acceptance separately from page or app health.
- **Unavailable and failure.** Invalid URL stays inline. Catalog entries are platform-specific and typed; a non-catalog executable path refuses. Offline shows no queue and no automatic retry. Launch accepted is not page healthy or app healthy. No developer transport terminology appears.
- **Focus.** Focus starts in the URL field or the catalog list. The two actions are mutually exclusive focus primaries. Cancel leaves no launch job.

#### M13 Send Files

- **Layout.** Native file picker summary with names, byte counts, and private destination. Exact targets and verified intended local user under D29. Label For verifiedUser onComputer per target. Window 560 wide.
- **Actions.** Send Files. Cancel per target. Open Details for per-target state.
- **Unavailable and failure.** Separate states Uploading, Stored, Waiting Computer, Waiting Recipient, and Delivered. Recipient Unavailable blocks targets without a verified binding. Waiting for Computer means unreachable; Waiting for Recipient means the intended user is absent. Resume only for that same user before expiry. No shared or next-user fallback. Per-target progress, cancel, identity, and quota failures are explicit. Committed jobs continue after the sending console closes; unfinished uploads do not. Quota and storage failures keep the safe draft of names, not file bytes.
- **Focus.** Focus starts on the file picker summary. Per-target cancel is a focus stop. Close during upload asks about unfinished upload only when one exists; committed jobs are named as continuing.

#### M14 File Request

- **Layout.** Short title and instructions fields, allowed file rules, eligible computer users, and close policy. One form container. No course or assignment hierarchy.
- **Actions.** Send Request. Save Draft sends nothing. Close Request retains submissions and requires replacement confirmation when replacing.
- **Unavailable and failure.** Empty required fields validate inline. Closing a request refuses new submissions and retains completed files. Independent pause of collection is separate from close. Draft retention after failure never dispatches.
- **Focus.** Focus starts in the title field. Save Draft and Send Request are distinct focus stops. Close Request is destructive and defaults to Cancel.

#### M15 Received Files

- **Layout.** Split request, submission, and file table with verified attribution. States Received, Pending, and Failed. One results container. Window 720 × 460.
- **Actions.** Open Files or Save Files through explicit native pickers. Export metadata only where authorized.
- **Unavailable and failure.** Safe explicit open and export only. No home-directory browser and no automatic extraction. Quarantine states are visible. Unverified attribution is shown as unverified rather than guessed. Failed rows offer safe Retry or Check Unknown. Isolated submission IDs never merge across users.
- **Focus.** Focus starts on the table. Open and Save are separate primaries for the selected row. Destructive removal defaults to Cancel. No focus theft when results update.

#### M16 Screenshot

- **Layout.** Native save sheet naming target, display, and timing evidence. Compact confirmation of the authority used.
- **Actions.** Save Screenshot. Cancel.
- **Unavailable and failure.** Screenshot authority is distinct from preview authority. Current display identity is required. No cached invalid export. Cancel retains no job and no partial file. Permission denial names the missing permission and does not fall back to a cached frame.
- **Focus.** Focus starts on the save sheet filename. Cancel is the default safe action. Sheet dismissal cleans any pending capture request.

#### M17 Computer Action

- **Layout.** Operation-specific confirmation listing exact targets and unsaved-work consequences. Typed operations: Wake, Lock, Log Off, Restart, Shut Down. One confirmation container. Window 720 × 460 for multi-target results.
- **Actions.** Confirm the typed operation. Cancel. Reconcile unknown outcomes before destructive retry.
- **Unavailable and failure.** Conflicting states are reviewed before dispatch. No offline destructive queue; offline targets are excluded with names. Wake Sent differs from Online. Unknown outcomes require Check or Reconcile Computer. No automatic background restart after cancel. Partial results show original targets and exact per-computer results.
- **Focus.** Focus starts on Cancel for destructive operations. The confirm action is labeled with the exact verb. Result list is a separate focus region with retry stops only where safe.

#### M18 Computer Details

- **Layout.** Name first, then platform, user, readiness, and available actions. Technical disclosure is collapsed. One details container.
- **Actions.** View Screen or a named support route. Open technical disclosure. Copy identifier only where useful and safe.
- **Unavailable and failure.** Local permission problems name the exact recovery, including remote Mac permissions that need local IT or user action. Unsupported capability gets a contextual explanation and no weaker fallback. Missing optional fields are omitted rather than shown as empty placeholders.
- **Focus.** Focus starts on the computer name heading region, then available actions. Disclosure is a focus stop. Closing details does not affect observation.

#### M19 Action Results

- **Layout.** Frozen-target durable per-result table with filters. Original scope remains visible. Window 720 × 460.
- **Actions.** Safe Retry Failed. Check Unknown. Metadata-only export.
- **Unavailable and failure.** Retry Failed never retries unknown or destructive-unsafe rows. Check Unknown opens reconciliation rather than guessing. Export excludes content, screens, and keys. Results survive leaving the screen. No fabricated success rows.
- **Focus.** Focus starts on the filter controls, then the table. Safe Retry and Check Unknown are separate focus stops with distinct labels. Export is secondary.

#### M20 Pause Monitoring

- **Layout.** Direct action when only viewing. Consequence sheet when this console owns control or active dependent work. One confirmation when needed.
- **Actions.** Pause Monitoring. Cancel to keep monitoring.
- **Unavailable and failure.** Pause ends this console's viewing and control and clears relevant pixels. Unrelated observers continue independently. Owned control is released with stated consequences. Pending release remains visible afterward. Resume requires fresh admission.
- **Focus.** Direct path needs no dialog. Consequence sheet defaults to Cancel. Focus lands on Resume after pause completes.

#### M21 Monitoring Paused Result

- **Layout.** Readiness inventory with cleared images and an unresolved-release link only when needed. Same grid structure as M02.
- **Actions.** Resume Monitoring. Open the unresolved-release link.
- **Unavailable and failure.** No forced report or classroom summary. Resume failure names the stage. Cleared images are never restored from cache. Unresolved release survives until acknowledgment or safe expiry.
- **Focus.** Focus lands on Resume Monitoring. The release link is a separate focus stop when present.

#### M22 Restrictions

- **Layout.** Expanded whole-device Restrictions editor for named policy bundles with duration and frozen-computer preflight. Apps via verified publisher or hash. Domains and subdomains without arbitrary encrypted URL paths. One bundle editor container.
- **Actions.** Test One. Apply. Remove Restrictions. Select named bundle and duration.
- **Unavailable and failure.** Real states Applied, Partially Applied, Not Enforced, Expired, Removing, and Cleanup Required. No force-kill of running apps by default. Temporary expiry and surviving cleanup are explicit. Local IT recovery is available. External policies are preserved. Helicoid-owned enforcement is stated honestly; MDM or browser-only enforcement is not claimed as whole-device. macOS uses EndpointSecurity execution and NetworkExtension with signed filter entitlement gating. Windows uses AppLocker user rules and WFP, with no claim of a robust security boundary. Prove VPN, proxy, encrypted DNS, IPv6, shared IP, restart, and conflicts; if any cannot be proven, the capability is unavailable. Failure to apply never leaves a half-applied silent state.
- **Focus.** Focus starts on the bundle list. Test One and Apply are separate primaries. Apply is destructive-adjacent and does not default. Preflight results are a focus stop before Apply becomes enabled.

#### M23 Recording and Audio

- **Layout.** Expanded deliberate local MP4 recording of one admitted display, with optional remote computer sound off by default. Separate permissions and disclosure rows. Destination and maximum duration fields. Active state shows elapsed time and Stop.
- **Actions.** Start Recording. Stop. Choose destination and maximum duration. Acknowledge computer-user disclosure.
- **Unavailable and failure.** No microphone, talkback, or server archive. Finishing continues until the file is committed. Disk full, permission revoke, disconnect, and display change each stop and finalize recoverably. No automatic restart. Close or quit during recording requires review of the unfinished file. Missing recording permission names the exact OS permission and does not fall back to screenshot.
- **Focus.** Focus starts on destination. Start Recording is disabled until permissions and disclosure are satisfied. Stop is persistent and always reachable during recording. Quit review defaults to Cancel.

#### M24 Remote Access

- **Layout.** Expanded public exposure, network, and authentication configuration. Own trust, carrier, relay, and certificate health. Exact direct and relay tests. Preserves 57.131.161.27 with ports 80 and 443 unchanged. One configuration container.
- **Actions.** Run direct and relay connectivity tests. Enable or disable public exposure explicitly. Open certificate and relay health.
- **Unavailable and failure.** Public is current direction, not delivered. Single-VM candidate is not verified and is labeled candidate. UDP-blocked TCP 443 must be proven exactly. No public fallback to weaker authentication. No takeover of occupied ports 80 or 443. Unused listeners are closed. If the exact route cannot be proven, the capability stays unavailable.
- **Focus.** Focus starts on the exposure control. Tests are secondary actions. Enabling public exposure requires a review step and does not default.

#### M25 Remote OS Sign-in

- **Layout.** Expanded authorized unattended OS sign-in. Preflight gate before ephemeral credential entry. Separate administrator-enabled capability. Resulting physical desktop and fresh view-only admission are shown explicitly.
- **Actions.** Run preflight. Enter ephemeral credential after preflight passes. Start one bounded attempt. Open resulting desktop in view-only.
- **Unavailable and failure.** No pre-login ordinary input or pixels. Fresh policy authentication and an exact encrypted endpoint channel are required. No secrets in logs, storage, or queues. One bounded attempt only. No control restore after sign-in. Windows CredentialProvider feasibility retains system recovery. macOS uses only a reviewed supported login adapter feasibility path; authentication services verify-credentials is not inferred as sign-in. Initial FileVault and preboot are excluded. If the adapter fails, show actual Unavailable with a named recovery route. No injection or other transport fallback.
- **Focus.** Focus stays on preflight until it passes. Credential fields appear only after preflight and clear on completion or failure. Attempt start is a single primary and does not auto-repeat.

### Monitored computer

#### E01 Enroll Computer

- **Layout.** Supplied profile or invitation and verified deployment identity. One enrollment form. Restrained HS mark above the task heading. Window 520 × 360 adaptive.
- **Actions.** Enroll. Import profile. Enter invitation manually. Open recovery route.
- **Unavailable and failure.** No premature capture or monitoring before enrollment completes. Wrong, revoked, consumed, or expired identity has a named IT recovery route and never silently retries. Secrets clear on completion or failure.
- **Focus.** Focus starts in the first empty enrollment field. Recovery route is a separate focus stop. Cancel leaves no partial enrollment claim.

#### E02 Pending Approval

- **Layout.** Deployment name, computer name, and one clear status line. One status container. Window 420 × 260.
- **Actions.** Check Approval. Cancel where allowed. Contact IT.
- **Unavailable and failure.** No premature monitoring while pending. Approval failure or revocation replaces the status with a named state and a recovery route. Repeated Check Approval is rate-limited honestly.
- **Focus.** Focus lands on Check Approval. Contact IT is secondary. Status updates do not steal focus from another window.

#### E03 Permissions

- **Layout.** Two concise permission rows only: Screen Recording for view screen, and Accessibility for keyboard and mouse. Plain symbol status per row. Window 600 × 300 top-aligned. No gear buttons, footer essays, or status pills.
- **Actions.** Allow where the platform can prompt. Open Settings when actual OS action is required. Contact IT. Recheck.
- **Unavailable and failure.** Show actual process permission readiness, never an optimistic grant. Windows shows native readiness and does not use Mac permission names. Denial keeps the row visible with a named recovery. Revocation is reflected on recheck. If another Allow request cannot advance a denied permission, expose a contextual text action to the correct System Settings pane. Re-query on return; never invent a denied or restart-required state from a Boolean probe.
- **Focus.** Each incomplete row exposes one current action: Allow or Open Settings. Focus starts on the first incomplete row; returning from System Settings refreshes readiness without moving focus. A confirmed grant removes the row action.

#### E04 Ready

- **Layout.** Small tray or menu status with Connected and No Active Screen Access. Deployment identity available on demand. Window 420 × 260. No lesson or class state.
- **Actions.** Open Status. Open Settings. Quit. Optional Details and Retry.
- **Unavailable and failure.** Unconfigured state offers Set Up as the only functional route. No disabled Help form or worker identifiers in the ordinary layout. Status Details shows actual values only. Quit and disclosure loss must safely end integrated work when present.
- **Focus.** Menu opens with focus on Open Status. Details is a focus stop. Quit confirmation explains actual owned activity and defaults to Cancel when work would be lost.

#### E05 Observation Active

- **Layout.** Actual operator identity or named list of observers. Current connection state. One identity group. No focus stealing on update.
- **Actions.** Request Help. Open Details. Dismiss the status surface without ending observation.
- **Unavailable and failure.** Identity updates on authority or desktop change. Unknown observer is labeled Unknown, never guessed. Loss of observation authority clears the active state immediately and returns to E04.
- **Focus.** Status updates never steal focus. Request Help is the primary user action. List updates preserve the focused row when possible.

#### E06 Control Active

- **Layout.** Persistent controlling identity and IT contact route. One controller named. Observers listed separately when present.
- **Actions.** Contact IT. Open Details. No computer-user Stop Control under D09.
- **Unavailable and failure.** Expiry, revocation, and session change release automatically and visibly. Competing owner shows a conflict state. Protected desktop or missing input permission names the exact cause. Teardown is never implied by a timer alone.
- **Focus.** Focus is never stolen by controller changes. Contact IT is the primary user action. Status surface does not trap focus.

#### E07 Help Request

- **Layout.** Optional bounded text field and requested or cancel state. One compact form.
- **Actions.** Request Help. Cancel Request.
- **Unavailable and failure.** Deduplicate repeated taps. Transport-required failures name the missing capability. Cancel is available while the request is outstanding. Oversized text validates inline.
- **Focus.** Focus starts in the text field. Request Help is the primary. Cancel Request is a separate stop while active.

#### E08 Message

- **Layout.** Accessible operator-named text window. Plain message body. Bounded inbox list when multiple. No repeated focus theft.
- **Actions.** Dismiss. Open earlier messages when present.
- **Unavailable and failure.** Wrap and scroll for long messages. No inferred read receipt. Unknown sender is labeled by the verified operator name available at delivery. Empty message never opens a window.
- **Focus.** Opening a message does not steal focus from the user's current work. Dismiss is the single action. New messages do not re-focus an already-open window.

#### E09 Screen Sharing

- **Layout.** Source and operator identity with one fitted presentation. Explicit fullscreen only where offered and tested. One presentation surface.
- **Actions.** Minimize presentation. Restore needs current authority. No computer-user Close or Stop Presentation under D30.
- **Unavailable and failure.** Denied native, menu, shortcut, accessibility, and API dismissal are enforced. Owner or scoped IT stop uses management surfaces. Minimize is not Receiving on screen. Lost source, user change, or expiry clears content with a named state. No Attention or control stop right is implied. No reconnect resurrection after teardown.
- **Focus.** Presentation does not trap focus. Minimize is reachable by keyboard. Fullscreen exit uses the platform's standard shortcut and is not intercepted.

#### E10 Attention

- **Layout.** Plain fullscreen message and owner name. No decorative chrome. One message surface.
- **Actions.** Operator release. Expiry. Authorized IT recovery.
- **Unavailable and failure.** No fake OS lock label. Actual platform restriction is stated separately. Watchdog recovery is available to authorized IT. Expiry shows remaining time. Preserve native OS security surfaces.
- **Focus.** Message surface does not trap keyboard focus. Standard OS escape and shortcut behavior remains local.

#### E11 Files and Requests

- **Layout.** Only this verified intended local user's sent files and separate private request and submission state. Native picker for submit. Window 520 × 360 adaptive.
- **Actions.** Submit Files. Replace submission with explicit confirmation. Save received files where permitted.
- **Unavailable and failure.** Confirmed D29 private delivery and final D21 close and replacement rules. Logout or user change fences upload and another user's state. Uploading is not Submitted. Sent files never expose request submissions. Queued files wait for the same user to return before expiry; a new user cannot become their recipient. No shared computer inbox.
- **Focus.** Focus starts on the file list or submit control. Replace requires explicit confirmation and defaults to Cancel. User change clears focus from another user's controls.

### Administration

#### A01 Overview

- **Layout.** Deployment name and an outstanding-work list only. One list container. No wall of healthy metrics.
- **Actions.** Open relevant approval or problem. Refresh outstanding work.
- **Unavailable and failure.** Empty outstanding work shows a quiet empty state, not a celebration. Failed refresh names the failure and keeps the last known list marked as last known.
- **Focus.** Focus starts on the outstanding-work list. Each row is a focus stop that opens its relevant screen. No focus theft on refresh.

#### A02 Computers

- **Layout.** Searchable native inventory table with columns Name, Platform, Enrollment, Readiness, Last Seen, and Version. No previews in this IT inventory. Window 1040 × 720.
- **Actions.** Add Computers. Open details. Search and sort.
- **Unavailable and failure.** No auto-observation from this screen. Unavailable rows keep name and last known metadata. Stale Last Seen is labeled. Search failure retains the query.
- **Focus.** Focus starts in search. Table rows are keyboard reachable. Add Computers is secondary. Details does not start observation.

#### A03 Enrollment

- **Layout.** Sections for Pending, Invitations, Approved Managers, and Identity Review. Exact verified identities only. Identity Needs Review details on demand.
- **Actions.** Approve. Reject. Create Invitation. Scoped IT Replace Identity.
- **Unavailable and failure.** No self-approval. Competing copied keys quarantine and require review. Replacement permanently retires the old key and reviews access before fresh approval. Bounded invitations expire honestly. Revoked managers show revocation time.
- **Focus.** Focus starts on Pending. Approve and Reject are distinct destructive-aware stops; Reject defaults to Cancel. Replace Identity is a separate review flow.

#### A04 Computer Access and Groups

- **Layout.** Explicit computer permissions with effective source, and separately a flat organization group editor. Two sibling sections, not nested cards. Staging groups freeze IDs.
- **Actions.** Save Access. Edit Group. Review effective permission and delegation.
- **Unavailable and failure.** Explain exclusions. Self or security changes use A09's authorized review. Membership never grants access. Staging groups grant no future access. Stale editor state warns before save and cannot silently overwrite. Revocation is reviewed before apply.
- **Focus.** Focus starts on the access table. Save Access is the primary for the access section. Edit Group is the primary for groups. Unsaved changes keep a safe draft and use Keep Editing or Discard Changes.

#### A05 Accounts

- **Layout.** Named local accounts with scope, MFA state, and status. External accounts listed separately. Recovery state per account. Window 1040 × 720.
- **Actions.** Create. Disable. Reset. Manage MFA. Start recovery-only verification.
- **Unavailable and failure.** All actions stay within account and privilege ceilings. Reset starts recovery-only verification and never shows a usable target password or session. Failed invalidation is Not Saved. The last recoverable security administrator is protected from lockout. No recovery media authority is created.
- **Focus.** Focus starts on the account list. Create is secondary. Reset is destructive and defaults to Cancel. Recovery-only fields clear on completion.

#### A06 Policies

- **Layout.** Grouped Access, Privacy, Sessions, Files, Restrictions, Recording, and Advanced. Each group one container. Effective source and version shown per group. New modules are off until configured.
- **Actions.** Save per group. Validate scope. Review Saved, Delivered, and Applied revisions separately.
- **Unavailable and failure.** Validation failures stay inline. Saved is not Delivered and Delivered is not Applied. Unavailable enforcement is labeled Not Enforced. Policy loss on persistence failure shows Security State Unavailable and stops new authority while safety cleanup remains effective.
- **Focus.** Focus starts on the first group with unsaved changes, otherwise the first group. Each group's Save is a local primary. Revision labels are not focus traps.

#### A07 Approved Apps and Websites

- **Layout.** Editable typed catalog with platform column and revision. Apps and Websites as sibling lists. One catalog container.
- **Actions.** Add typed entry. Edit. Test One. Activate or deactivate.
- **Unavailable and failure.** No shell or privileged path entries. Catalog presence does not grant access. Test on one authorized computer before activation. Test failure keeps the entry inactive. Platform mismatch is labeled.
- **Focus.** Focus starts on the catalog list. Test One and Activate are separate stops. Activate requires a prior successful Test One in the current revision.

#### A08 Activity

- **Layout.** Searchable metadata table with actor, action, computer, time, and result. Filter controls above. Window 1040 × 720.
- **Actions.** Filter. Authorized metadata-only export.
- **Unavailable and failure.** No content, screens, message bodies, or credentials. Export is metadata only and records its scope. Empty filter results keep the query. Unknown outcomes are labeled Unknown.
- **Focus.** Focus starts in the filter search. Export is secondary and confirms scope before writing.

#### A09 Security

- **Layout.** Sections for trust, managers, MFA, enrollment, revocation, and recovery. Delegation and administrative details on demand. One security container.
- **Actions.** Fresh authenticated review of exact authority changes. Approve manager changes from an approved or local recovery context. Independent target account recovery. View recovery codes only for the verified recovering account.
- **Unavailable and failure.** Recovery codes are shown once and only to the verified target. No recovery media authority. Self-approval is refused. Stale authority state requires re-authentication. Security State Unavailable stops new authority and keeps safety cleanup reachable.
- **Focus.** Focus starts on the first outstanding security issue, otherwise trust. Recovery codes display is a modal with explicit dismiss and clears on close. Destructive revocation defaults to Cancel.

#### A10 Import and Export Configuration

- **Layout.** Native picker and a validated change preview before apply. One preview container.
- **Actions.** Export. Import. Apply after preview. Cancel.
- **Unavailable and failure.** Export excludes secrets. Import refuses unsupported or unsafe data before apply. Preview shows exact typed changes. No trust or access bypass through import. Partial apply reports Not Saved for the failed remainder.
- **Focus.** Focus starts on the picker. Apply is disabled until preview validates. Cancel discards the preview and any staged change.

#### A11 Identity Integration

- **Layout.** Separate provider-neutral OIDC and SCIM sections. Connector status, edit, and test per connector. One connector container. No embedded password capture.
- **Actions.** Configure connector. Test. Enable. Sync preview. Disable. Revoke.
- **Unavailable and failure.** External browser with PKCE only. Link by issuer and subject, never by email alone. No implicit computer access from identity sync. Sync preview can disable and revoke with review. Local recovery administrator is preserved. IdP MFA counts only when policy is verified. Test failure keeps the connector disabled and preserves current direct computer access.
- **Focus.** Focus starts on the connector list. Test is primary before Enable. Enable requires a successful test in the current revision. External browser handoff does not trap focus on return.

### Server setup and operation

First-time setup uses the shared Manager interface, with local-host choices selected for its target platform and Linux SSH available from both builds. Routes join at host checks. Setup finishes at Server Ready for Enrollment. Deployment Verified is separate later evidence and never blocks finishing setup.

| Shared Manager build | Available routes | Choose Hosting content |
| --- | --- | --- |
| Qt Manager on macOS | macOS local or Linux SSH | This Mac or Existing Linux Server; Windows local hosting is available from the Windows build |
| Qt Manager on Windows | Windows local or Linux SSH | This Computer or Existing Linux Server |

Windows local and macOS local follow H01 to H02 to H05. Linux SSH from either Manager build follows H01 to H03 to H04 to H05. All then follow H05 through H12. The macOS local route requires an actual product-owned launchd service and installer evidence; remote Windows installation remains unsupported.

Setup windows use 600 × 400 unless noted. Small work areas scroll. Bottom actions stay reachable while the body scrolls.

#### H01 Choose Hosting

- **Layout.** Only the routes available to the current manager app, as listed above. One route list. Restrained HS mark above the task heading. Optional containers are marked advanced.
- **Actions.** Continue with the selected route. Open advanced container options.
- **Unavailable and failure.** Unavailable routes are omitted, not shown disabled with developer copy. Wrong-app guidance for Windows hosting is inline and factual. No secrets are collected until the chosen packages are available.
- **Focus.** Focus starts on the first available route. Continue is the primary. Advanced options do not steal focus.

#### H02 Local Host

- **Layout.** Both local hosting routes: This Mac in Mac Manager, This Computer in Windows Manager. Show the actual host name, OS, architecture, product install scope and native authorization status. No SSH identity fields.
- **Actions.** Check This Computer. Mac uses actual Service Management registration/approval; Windows uses actual elevation/service prerequisites.
- **Unavailable and failure.** Missing approval or elevation blocks writes and names the next action. Occupied/conflicting paths are reviewed. Package unavailability stops the wizard before administrator or recovery details are collected.
- **Focus.** Focus starts on the actionable approval status when needed, otherwise Check This Computer. Host summary is not a focus trap.

#### H03 Connect

- **Layout.** Linux route only. Server address and selected secure SSH authentication with key agent and default advanced port. One connection form.
- **Actions.** Continue to host identity. Select authentication method. Open advanced port settings.
- **Unavailable and failure.** No writes before identity is verified. Authentication failure keeps the address and method. Key agent absence is named. Invalid address stays inline.
- **Focus.** Focus starts in the address field. Authentication method follows. Continue is disabled until a method is selected.

#### H04 Host Identity

- **Layout.** Linux route only. Actual SSH host identity and trusted-source explanation. One identity group.
- **Actions.** Trust Host after verification. Cancel.
- **Unavailable and failure.** Changed key or refusal performs no writes and shows a blocking review. There is no trust-anyway action. Verification failure keeps the prior trusted state untouched.
- **Focus.** Focus starts on the identity status. Trust Host enables only after verification text is populated. Cancel is the default safe action.

#### H05 Check Host

- **Layout.** Platform-specific rows for OS, package, privilege, storage, path, service, and port. States Passed, Needs Attention, and Checking. Expanded failure details on demand. One checklist container.
- **Actions.** Continue when required checks pass. Open expanded details. Re-run a failed check.
- **Unavailable and failure.** No fake green rows. A check without evidence is Not Verified, not Passed. Conflicts and alternatives are named. Unavailable required checks block Continue. Checking uses honest indeterminate progress.
- **Focus.** Focus starts on the first Needs Attention row. Continue enables only when required checks pass. Details expand inline without leaving the checklist.

#### H06 Network

- **Layout.** Address, product port, and intended LAN scope. Listener and firewall summary. One network form. HTTPS 8920 is the LAN primary.
- **Actions.** Validate listeners and firewall. Review product-scoped changes. Continue.
- **Unavailable and failure.** No occupied port 80 or 443 takeover. Unused listeners are closed. Public exposure is deferred to M24 and is not configured here. Validation failure names the conflicting owner. Firewall changes require explicit review before apply.
- **Focus.** Focus starts on the address field. Validate is secondary. Continue enables after successful validation.

#### H07 Deployment Identity

- **Layout.** Display name and the actual generated trust identity. Public connection details summary. One identity form.
- **Actions.** Save public connection details. Continue. Review identity replacement.
- **Unavailable and failure.** Name and public trust only for actually created identity. Export public profile contains no fabricated identifier. Restore is a separate flow. Identity replacement explains consequences and requires confirmation.
- **Focus.** Focus starts on the display name field. Save is the primary. Replacement confirmation defaults to Cancel.

#### H08 Administrator

- **Layout.** Named local account, required MFA, and recovery handling. One administrator form. Initiating manager is shown.
- **Actions.** Continue only after real secure setup. Configure recovery codes.
- **Unavailable and failure.** Mandatory administrator MFA cannot be skipped. Recovery codes are shown once to the verified administrator. No usable password is displayed after creation. Bootstrap is local or tunnel only. Secrets clear on completion.
- **Focus.** Focus starts on the account name field. MFA step receives focus next. Continue is disabled until setup is real and complete.

#### H09 Storage and Backups

- **Layout.** Paths, limits, and optional encrypted backup destination with separate keys. One storage form.
- **Actions.** Validate writable space, quota, and recovery. Continue.
- **Unavailable and failure.** Dedicated product paths only. Storage checks fail closed. Backup destination without a separate key refuses. Quota exhaustion is named before Continue.
- **Focus.** Focus starts on the primary storage path. Validate is secondary. Backup fields are a separate focus group.

#### H10 Review

- **Layout.** Exact service, files, listeners, firewall, storage, and startup changes. One review list. Destructive rows are labeled.
- **Actions.** Install. Back preserves safe inputs.
- **Unavailable and failure.** Review shows only owned resources. Recheck before writes. Unknown or unverified rows block Install. Back never discards validated non-secret input.
- **Focus.** Focus starts on the review list. Install is the primary and is labeled Install, not Continue. Back is a separate safe stop.

#### H11 Install

- **Layout.** Actual current phase and measurable progress. Phase journal is visible. One progress container.
- **Actions.** Safe cancel by phase. Retry failed phase. Rollback by phase boundary.
- **Unavailable and failure.** No fake install progress. Partial changes are identified by phase. Cancel before Install changes nothing. Later cancel reports completed effects and remaining cleanup. No automatic background restart. Journal is real, not decorative.
- **Focus.** Focus stays on the phase list. Cancel is always reachable. Retry appears only for retry-safe phases.

#### H12 Ready

- **Layout.** Server Ready for Enrollment with healthy service, identity, administrator, storage, listeners, and enrollment path. Deployment not yet Verified is stated. One status container.
- **Actions.** Create Profile. Open Administration. Export public profile.
- **Unavailable and failure.** Deployment Verified is separate and never blocks Ready. No tour. Unhealthy prerequisites return to their phase with named recovery. A registered helper, running process, or reachable port alone does not produce Ready.
- **Focus.** Focus lands on Create Profile. Open Administration is secondary. Export is a separate stop.

#### H13 Verify Deployment

- **Layout.** Checklist for one real enrolled endpoint with authorization, authorized media, denial, teardown, and recovery. Evidence rows show source, artifact, and platform. One checklist container.
- **Actions.** Run checklist. Open evidence detail. Retry a failed row.
- **Unavailable and failure.** Reachability alone is insufficient. Process is not verified. Never required before Ready. Each row is Verified, Failed, or Not Verified with named evidence. No process-equals-verified claim.
- **Focus.** Focus starts on the first Not Verified row. Run checklist is primary. Evidence detail is a focus stop.

#### H14 Server Status

- **Layout.** Process state, authority readiness, Ready or Verified state, and outstanding issues. Storage Low and Security State Unavailable open scoped recovery. One status container. Window 420 × 260 when compact.
- **Actions.** Start. Restart. Drain. Open scoped recovery. Open Details.
- **Unavailable and failure.** Process Running does not mean authority is healthy. Consequences are stated before Restart and Drain. Safety stop remains reachable. Scoped storage recovery precedes new authority. Durable reconciliation is required before resuming after Security State Unavailable.
- **Focus.** Focus starts on the first outstanding issue, otherwise process state. Restart and Drain are destructive and default to Cancel. Safety stop is always a reachable focus stop.

#### H15 Backups

- **Layout.** History list with selected contents, destination, and result. One history container.
- **Actions.** Back Up Now. Open a history row. Configure retention.
- **Unavailable and failure.** Bounded encrypted retention with explicit policy. Honest integrity result. Previous backups are preserved when a new backup fails. Retention deletion is separate from backup failure.
- **Focus.** Focus starts on the history list. Back Up Now is the primary. Retention configuration is secondary.

#### H16 Restore

- **Layout.** Backup verification, compatibility, keys, destination, trusted history, old-host fencing, and replacement authority review. Per-peer trust state: Trust Updated, Pending Trust Update, or Needs Local Recovery. One restore container.
- **Actions.** Restore quarantined. Independently authorize cutover. Review per-peer trust. Cancel.
- **Unavailable and failure.** Restored identities remain inactive until complete trusted later history or independent fresh IT validation. Offline peers remain Pending Trust Update. Offline or unfenced predecessors cannot be marked complete. Cancellation cannot reactivate retired authority. No resurrection of retired authority. Recovery quarantine is enforced.
- **Focus.** Focus starts on verification status. Cutover authorize is a separate destructive review and defaults to Cancel. Per-peer rows are focus stops.

#### H17 Updates

- **Layout.** Verified compatible permitted version and per-computer readiness. Separate scoped Roll Back review. Signed hash and fresh compatibility rows. One update container.
- **Actions.** Pilot. Schedule. Install. Roll Back exact artifact.
- **Unavailable and failure.** Refuse below-policy or stale releases even with a valid signature. Security floor is never lowered. Authorized functional rollback uses an exact artifact and preserves the floor. Offline import requires fresh trusted metadata. Actual phases are shown honestly. Readiness failures name the computer.
- **Focus.** Focus starts on the release list. Pilot is primary before broad Install. Roll Back is a separate review flow and defaults to Cancel.

#### H18 Support and Recovery

- **Layout.** Guided issues with disclosed technical diagnostics. Redaction preview before save. Repair manifest for storage and commit state. One recovery container.
- **Actions.** Run Check. Save Redacted Report. Apply repair manifest. Open endpoint local recovery.
- **Unavailable and failure.** Hub-independent endpoint recovery is available. Repair required storage, commit, or integrity state before admitting new work. Never reset trust or delete unrelated files to hide an error. Durable probe and storage integrity are verified before resume. Preserve data by default.
- **Focus.** Focus starts on the issue list. Run Check is primary. Redaction preview is a focus stop before Save. Repair apply defaults to Cancel.

#### H19 Remove Server

- **Layout.** Product-owned resources list with keep-data default selected. Deletion is a separate explicit intent. One confirmation container.
- **Actions.** Remove with keep-data default. Separate explicit data deletion. Resume partial removal.
- **Unavailable and failure.** Keep data is the default. Deletion requires a separate confirmation naming exact paths. Backups are preserved by default. Partial removal is resumable and reports completed effects. No silent data loss.
- **Focus.** Focus starts on Cancel for the destructive confirmation. Keep-data is the default selected option. Separate deletion intent has its own confirmation and focus entry.

### Settings and help

Settings windows use 640 × 440 unless noted. About uses 360 × 340.

#### P01 General

- **Layout.** Default Grid or List view and preview size. Appearance follows the system. One preferences container. Presentation preferences only, not deployment metadata.
- **Actions.** Change default view. Change preview size. Rely on system appearance and accessibility settings.
- **Unavailable and failure.** Invalid stored values fall back to documented defaults without error dialogs. Preview size is remembered per real account and deployment. No theme parallel that fights the system.
- **Focus.** Focus starts on the first preference control. Native settings behavior applies. Changes apply immediately and do not need a Save button.

#### P02 Server and Account

- **Layout.** Trusted server, current account, and roles. Disclosed identity. One account container.
- **Actions.** Change Server. Sign Out. Release switch.
- **Unavailable and failure.** Sign Out and Change Server release local authority and clear caches between identities. Active owned work is explained before release. No cache survives an identity switch. Failed sign-out keeps the session marked as requiring re-authentication.
- **Focus.** Focus starts on the account summary. Sign Out is destructive and defaults to Cancel. Change Server is a separate review flow.

#### P03 Notifications

- **Layout.** Task notifications and optional sound. Essential active-control identity remains visible independently. One notifications container.
- **Actions.** Enable or disable task notices. Enable or disable sound.
- **Unavailable and failure.** Respect OS settings. Essential disclosure cannot be disabled. Missing OS permission for notifications is named with a recovery route. Disabling notifications never hides active observation or control identity.
- **Focus.** Focus starts on the first toggle. Toggles are independent focus stops. No Save button.

#### P04 About

- **Layout.** App icon from the target's compiled icon, approved display name, version, company identity, source, license, acknowledgments, and release state. Plain verified update status. Window 360 × 340. Ordinary compact window, not floating.
- **Actions.** Open corresponding source. Open license. Open acknowledgments. Check for updates where implemented.
- **Unavailable and failure.** No promised unimplemented update channel. Update status is plain and verified. Missing source link omits the control rather than showing a dead one. General monitoring identity, not school branding.
- **Focus.** Focus starts on the version row. Source and license links are focus stops. No focus trap in the About window.

#### P05 Help

- **Layout.** Task guides for Connect, Monitor Computers, Remote Assistance, and Contact IT. Search over actual shipped help. IT and diagnostics routes. Shortcut help. One help window, not a teaching sidebar.
- **Actions.** Open a guide. Search. Open diagnostics. Open shortcut help. Contact IT.
- **Unavailable and failure.** Search covers only actual shipped content. No promised unimplemented features. Empty search keeps the query. Diagnostics disclose technical detail on demand only. Shortcut help lists only real commands including Stop Control and Control-Tab.
- **Focus.** Focus starts in help search. Guide list follows. Shortcut help is a separate focus region. Help close returns focus to the previously focused window.

## Cross-screen states

| Situation | Visible state and recovery |
| --- | --- |
| Connecting and loading | Keep task context; honest progress, no fabricated computer screens |
| Empty inventory | No Computers; enrollment or contact route, no required group setup |
| No search matches | Keep query; Clear Search; underlying inventory and inclusion remain; temporary tile selection has cleared |
| Computer offline | Stable name; Offline; clear stale frames |
| Stream stale | Updates Delayed after 3 seconds without stream health; static desktop is not stale solely for no new pixels; clear after 5 seconds or on invalidation |
| Permission missing | Name the missing local permission and still-working capabilities |
| No desktop user | No User Signed In; no automatic remote login |
| Partial operation | Original targets and exact per-computer results |
| Outcome unknown | Check or Reconcile Computer before destructive retry |
| Server disconnected | Reconnecting to Server; bounded authority expires and frames and restrictions release |
| Required persistence failed | Security State Unavailable; the attempted change is Not Saved or the effect is Outcome Unknown. Stop remains usable; scoped storage recovery precedes new authority |
| Server cutover incomplete | Pending Trust Update or Needs Local Recovery per peer; Authority Needs Review for detected competing bindings; no automatic server switch |
| Identity or access changed | Stop affected work; Verify Server, Sign In Again, or Contact IT |
| Unsupported capability | Contextual explanation; no weaker fallback |
| Unsaved consequential editor | Keep Editing or Discard Changes; ordinary read-only close stays direct |
| Cancelled work | Report completed effects and remaining cleanup; no automatic background restart |
| Recovery | Restore context after fresh admission; no implicit control or recording; Identity Needs Review and quarantined identities never display pixels |
| Updates Delayed clear | Clear immediately on confirmed disconnect, authentication, desktop, or display invalidation, or earlier lease expiry; otherwise after 5 seconds of resumed health |
| Pending Release | Survives viewer close; visible until endpoint acknowledgment or safe expiry bound, never an arbitrary local timer |

## Lifecycle and coverage

The ordinary path is Connect, Verify, Sign In, then Computers with automatic authorized previews. Trusted and sign-in steps can be skipped only when current authority permits. Pause returns to readiness placeholders; Resume obtains fresh admission. Opening an IT-only inventory never starts observation.

Independent remote windows can outlive main-window closure. Native Quit, sign-out, and server-switch explain actual owned activity and release it. Crashes, sleep, and force termination use leases, not a guaranteed close callback. File requests and machine commands have their own direct authority; no hidden lesson object coordinates them.

The inventory contains exactly 74 screen families: C01–C03, M01–M25, E01–E11, A01–A11, H01–H19, and P01–P05. All 74 are specified in this source document. Implementation is ordered by the phases in the acceptance ledger. Specification coverage is not runtime acceptance.

Existing unit and build prerequisites are not end-to-end evidence. Production gates remain signing, native installed acceptance, Intel runtime, two-host recovery and cutover, and independent security review. The study and research ledgers use fictional computers, people, and schematic screens. Static source, link, and script checks do not prove native layout, accessibility, capture, permission enforcement, or installation behavior.
