# Workflow contracts

Draft updated on 10 October 2026. These are general computer-management workflows. [Decisions](decisions.md) and [system contracts](contracts.md) define authority; [ui.md](../ui.md) supplies screen IDs. Automatic authorized previews, optional simple groups, and the broader tools are confirmed.

## Shared dispatch rules

Every request resolves authenticated account, deployment, computer IDs, current permission revision, and actual desktop where applicable. Display names and group names are never authority.

Monitoring inclusion chooses overview computer IDs through All Computers or optional group/individual checkboxes. It is separate from temporary tile selection. New action scope is selected displayed computers, or all displayed computers after inclusion/search/status filtering when no tiles are selected. Displayed offline computers remain targets. Inclusion/search/status changes clear temporary selection. Sort/preview-size changes preserve selected IDs. A composer names its actual targets and freezes them on opening.

Opening an action composer freezes its computer IDs. Group edits, search, new enrollment, reconnect, and a selection change behind the dialog cannot expand that snapshot. An explicit scope change revalidates and updates the summary. Zero targets disables dispatch. An account can see a computer while lacking the permission for a particular action; exclusions are named before dispatch.

Disable duplicate submit while pending; keep safe cancellation/navigation usable. On ambiguous timeout, query the original request/action ID. Do not create another destructive operation to hide uncertainty. Saved configuration and drafts are separate from live authority. Unsent messages, input, and shutdowns never become a reconnect queue.

Required state must commit before authority or effects under the [failed-persistence contract](contracts.md#failed-persistence-and-storage-recovery). Not Saved, Security State Unavailable, and Outcome Unknown are different; database/audit failure must never disable safety stop/cleanup. V49 belongs to each affected write/dispatch boundary, not just backup testing.

E03/E06/E07 freeze numeric bounds before implementation: normalized text/display/UTF-8 limits; account/invitation/session limits; monitoring/stream capacity, leases and stale-image threshold; action deadlines/dedup/result retention; file/count/chunk/concurrency/storage quotas; setup reserve/relay/backup limits. Missing bounds block the owning slice. An implementation agent cannot choose unlimited allocation or an arbitrary silent constant.

## W01. Connect, verify, and sign in

Screens: C01 through C03, P02.

Enter an address or import a bounded public connection profile. Validate schema/scheme before connection. Discovery suggests a name/address only. Verify the actual server identity using the E07 trust mechanism and information supplied through a trusted organization channel. A changed stored identity stops access and offers deliberate verification/Contact IT.

Authenticate a named local account and required MFA through the organization-operated service. Apply server rate limits without account-enumeration errors. Clear secret fields on success/cancel; no credentials/tokens/factors in logs. Account and manager-device approval are separate checks. A manager needs one-time administrator approval under D20; pending/revoked identities cannot open computer access. Ordinary subsequent sign-in on the approved identity does not ask for approval again.

A current observation-authorized account opens Computers and automatically invokes W04. An IT-only account opens administration without screens. Cached computer names may be shown offline, but cached roles never create fresh authority. Sign-out/server-switch follows W15 and clears scoped secrets. Local sign-in works without a vendor account or internet identity provider.

Connect only to the peer's pinned active authority binding under the [cutover contract](contracts.md#active-server-authority-and-cutover). A copied/restored server, larger claimed generation, or changed address cannot authorize an automatic trust switch. Pending Trust Update/Authority Needs Review has a named independent recovery route and no new computer operations.

## W02. Enroll a computer

Screens: E01 through E03, A02, A03, A09.

1. IT creates a short-lived invitation/profile with trust identity, intended endpoint/manager role, expiry, allowed uses, and configuration. It contains no reusable private deployment key.
2. Generate a unique local identity through the selected key store. Cloned OS images generate keys after cloning; they do not duplicate an enrolled private identity.
3. Validate trust/profile/version. The server atomically checks key possession, invitation use, rate limits, and approval policy.
4. Pending means Waiting for Approval, with no observation/control authority. Administrator approval binds this exact identity to an inventory record.
5. Report actual OS desktop, permission, input, and version capability. Installed/online is not screen-ready.
6. Assign access directly to computer IDs through W16. Adding a computer to an organization group is not an access grant.

Consumed/expired/wrong/revoked invitations have a request-new-profile route. Duplicate identity cannot approve two independent records. Revocation stops fresh admissions and current access within the bounded lease contract. Identity repair is an explicit authorized recovery process. The ordinary computer user cannot enroll into another deployment or grant themselves permissions.

Apply the [identity competition contract](contracts.md#enrollment-identity-competition) to already-enrolled keys, not just enrollment requests. Competing authenticated incarnations/connections produce Identity Needs Review and quarantine, never automatic replacement. IT reviews the intended physical machine, permanently retires the copied key, provisions fresh identities, and explicitly reviews its access before activation. A second machine needs a separate record. Ordinary authenticated reconnect uses generation fencing and must not be misreported as a clone. V43 covers both paths and the documented detection limit.

Approval also enforces [administrative ceilings and bootstrap](contracts.md#administrative-delegation-and-account-recovery). An invitation/enrollment administrator approves only scoped exact identities, never viewing rights. The first manager can be bound through the verified single-use H08/H11 bootstrap; a later pending manager cannot approve itself. Use an already approved authorized context or independent local recovery and retain V46 evidence.

## W03. Inventory, simple groups, and approved catalogs

Screens: M01, M03, M04, A02, A04, A07, A10.

All Computers lists authorized inventory immediately. Groups are optional, flat named sets. A computer can belong to several groups; All Computers deduplicates by immutable ID. No seats, course dates, schedules, class membership, or required parent container.

Draft ownership rule: administrators manage shared groups; operators may manage their own personal groups from currently visible authorized IDs. Group editing is an organization preference capability, separate from computer access. Group membership changes cannot grant viewing/control, alter open action snapshots, or move an active independent remote window to another target.

Group/individual checkboxes change the included-ID set and overview stream demand. All Computers selects All mode; unchecking it selects an explicit empty set. Group checks add/remove currently authorized member IDs; mixed state means some member IDs are included. Multiple and overlapping groups deduplicate by computer identity. Persist All mode or explicit IDs per account/deployment, then restore against current authorized inventory. Filter/search changes clear temporary tile selection. Source-informed behavior and exact context-menu/range rules are in [ui.md](../ui.md#veyon-workflow-adapted-to-the-native-apps).

Explicit permission checks still happen for every computer. Unchecking stops that overview stream, not an independent detailed window or existing sharing/attention/file recipient. Removing/revoking an identity tears down affected access regardless of its old group membership. Changing an optional group name changes no command identity.

CSV import uses a native picker, bounded parse, field mapping, row errors, preview, and transactional Apply. Resolve immutable enrollment IDs; ambiguous names, duplicate identities, wrong-deployment and incompatible rows fail before mutation. Renaming labels does not change identity or old results.

Approved apps have IT-owned typed platform entries and constrained arguments. No teacher/operator shell field. Test an entry on one authorized target before activation. Import/export excludes secrets by default.

## W04. Open the authorized console and begin automatic previews

Screens: M01, M03, E04, E05. Confirmed D08.

1. After W01 and manager approval, resolve current computer inventory and per-computer view permissions. First use includes All Computers; later openings restore All mode or explicit included IDs against current access. A saved empty inclusion shows No Computers Included with Choose Computers, distinct from zero enrolled/authorized inventory.
2. Begin an operator-owned bounded monitoring session automatically. No Start Monitoring/Start Lesson button or confirmation blocks the normal path.
3. Render stable Connecting/readiness tiles first. Obtain fresh admission for each current desktop before its first screen frame. Overview previews use the endpoint's current primary display under D25; layout/primary changes require current display mapping and admission, with no silent alternate-display fallback. One offline/missing-permission computer does not prevent other previews.
4. Expose per-target Live, Offline, No User Signed In, Screen Permission Needed, or Unsupported Version. No simulated, cached, or unauthorized screen image is a live preview.
5. The monitored computer reports all actual authorized observer identities while viewing is admitted, and the unique controlling identity if present. One operator's pause does not end another viewer's admitted stream.
6. Schedule visible previews first with bounded resources. Offscreen/minimized UI may lower or stop preview demand, but independent remote windows retain their own admitted demand. Never trade correctness for unbounded decoder allocations.

Account/server failure prevents admission and offers recovery. Pause invokes W15; Resume explicitly restarts with fresh authority. A pause survives transient reconnect within this console and is not undone by a group/search change. A new authenticated application launch begins automatic monitoring unless a separately frozen safe startup setting says otherwise.

## W05. Reconnect, inventory change, and OS desktop change

Screens: M03, M18, M19, E04, E05.

A reconnecting preconfigured eligible computer can regain view-only previews automatically after fresh current-desktop admission. No room/lesson join exists. Unknown/discovered computers remain outside inventory and access until enrollment/configuration.

Draft inventory rule under D18: in All mode, current authorized inventory can introduce a newly eligible tile and request fresh observation authority automatically. Explicit-ID inclusion never silently adds a newly enrolled computer or changed group member. It cannot inherit a previous grant or become a recipient of an existing message, launch, restart, file action, sharing session, or attention command.

Connection loss preserves identity/selection/results, labels stale receipt age, and clears frames within the configured bound. User logout/switch/desktop replacement ends old observation/control, releases held input, and clears user-specific caches. A new desktop has a new identity and needs admission.

Resolve the exact eligible session through the [OS session selection contract](contracts.md#os-session-selection), never the first active worker. Under D26, Windows RDP and background users cannot substitute for an unavailable physical console; Mac background/virtual sessions cannot substitute for its primary console. Locks, attachment transitions, and ambiguous state clear old pixels and report the actual reason. S02 requires V40 evidence.

Revocation/identity change stops affected work. Server loss admits no fresh actions and expires current media/control/attention. Reconnect restores safe context and view-only where permitted, never remote input, control, recording, cancelled files, or expired operations.

## W06. View and assist one computer

Screens: M06, M07, M18, E05, E06. D09/D24/D25.

Single click selects. Double-click/Open View Screen creates a separate operator/manager-bound detail monitoring context for view-only observation of that computer. It does not rely on that computer remaining included in the overview. Take Control checks current desktop, permission/input capability, and unique controller ownership. Show Authorizing until endpoint acknowledgment, then Controlling and Stop Control. A competing controller produces a named conflict/contact/recovery route, not silent displacement.

Typed pointer/key/text events bind to interaction/generation. Disconnection discards queues. E04 must prove scaling, multiple displays, negative coordinates, rotation, Hungarian/US/non-Latin layouts, dead keys, modifiers, IME, and focus transitions. No keystroke log or arbitrary command conversion.

Only the explicitly focused remote canvas forwards input under the [input ownership contract](contracts.md#local-and-remote-input-ownership). Toolbar/menu/dialog/picker/other-app input stays local; opening those surfaces releases injected held input. Refocusing cannot replay local typing or restore expired control. D27 leaves the computer user's keyboard/mouse enabled. V42 proves native focus routing and overlapping local/remote input before S03 completes.

Take Control while sharing or attention is active follows the [conflict matrix](contracts.md#interaction-conflicts), including explicit own/IT-authorized replacement and teardown evidence. A new operation never clears another owner's claim just because this operator can control the computer.

Stop Control immediately stops local forwarding, requests teardown, releases held keys/buttons, and visibly returns to Viewing only if observation remains authorized. Closing the window ends its interactions. Stale acknowledgments cannot resurrect it. Focus loss cannot leave remote modifiers stuck.

Minimizing the remote window immediately stops forwarding/releases held input, cancels any pending Take Control, and releases active control while keeping authorized viewing active. If release is unacknowledged, retain Pending Release; neither restoring nor refocusing can reactivate that claim. After restore, show view-only and require explicit Take Control once previous release is reconciled. A delayed admission acknowledgment is fenced and torn down rather than applied. Minimize/restore cannot end another operator's independent viewing or control.

The managed computer shows controlling identity and IT contact, without a user stop button/menu/shortcut/API. The owner or authorized IT can stop; IT stop does not subscribe to pixels. Expiry, permission loss, user change, revocation, and relevant process failure still stop automatically. Windows protected desktop and Mac TCC limitations report capability failures; no elevation/weak-password fallback.

## W07. Share an authorized screen

Screens: M08, M09, E09.

Select a native supported display/window, or a separately authorized monitored-computer source. Freeze recipients and exclude recursive source-to-self paths where required. Selecting the source is not capture admission. Remote-source capture/distribution has separate permission and visible source disclosure.

Start Sharing admits source and receivers, with exact Receiving/Unavailable results. Packet transmission alone is not proof of display. E01/E06 must demonstrate encode-once per quality layer, bounded fan-out, bandwidth, and receiver costs.

Both source and receiving claims follow the [conflict matrix](contracts.md#interaction-conflicts). Review reverse transitions from attention/control/receiving as well as starting attention during sharing. Stop a conflicting source only after showing that all its recipients lose the share; stopping one recipient branch leaves the other recipients active.

Keep source/recipient identity and Stop Sharing visible. Changing source is explicit and increments source generation; a closed window does not silently become an entire-display share. Source/user/permission/authority loss ends it and clears receiver content.

Recipients stay fixed when groups/search/inventory change. Apply the [confirmed D30 receiver policy](contracts.md#screen-sharing-receiver-policy) before starting: one native fitted presentation, explicit fullscreen intent where offered, honest minimized/displayed state, and explicit stop only by the sharing owner or scoped authorized IT. Computer-user native/menu/shortcut/accessibility/API dismissal cannot end the branch. Authorized branch stop preserves other recipients and cannot release Attention or remote control; automatic safety teardown remains effective. No stopped branch auto-resumes; zero receivers ends distribution/source demand. V50 completes the native policy/race proof. Version 1 shares video; audio is a separate later sensitive module.

## W08. Attention

Screens: M10, E10.

Compose a bounded plain message, frozen targets, and policy-bounded duration. Validate empty/oversized text. A short renewable authority cannot become an indefinite offline lock.

Use the [conflict matrix](contracts.md#interaction-conflicts) symmetrically: attention replaces conflicting own control/sharing only after consequence review and acknowledged/fenced teardown. A foreign interaction requires explicit scoped IT stop authority plus attention permission, otherwise refuse. Cancel before teardown preserves prior activities; after teardown, cancellation cannot resurrect them. Per-target stop/apply acknowledgment exposes mixed results; there is no atomic whole-fleet transition claim.

The endpoint acknowledges only its actually supported restriction. A fullscreen window a standard user can leave cannot be advertised as tamper-proof lockdown. Release removes the product restriction, preserves OS authentication, and does not replay a stopped sharing session. A watchdog and authorized local IT recovery remain usable without the manager/server.

## W09. Message and help

Screens: M05, M11, M19, E07, E08.

Plain text only; reject whitespace-only/forbidden-control/over-limit input. Freeze recipients. Preserve failed unsent text for explicit retry, not an offline queue. Sent/server-accepted, delivered, displayed where measured, and unavailable are distinct. No inferred read receipt.

Messages name the operator, wrap/scroll, and Dismiss. Repeated messages cannot continuously steal focus or create unbounded windows. Freeze message-list capacity/overflow/order with E06. Sensitive text is not copied to audit.

Help binds to the current computer/desktop, not a class. Repeated taps query/update one unresolved request. Optional text is bounded. View Screen does not silently take control or resolve help. Operator Resolve and user Cancel are explicit. Desktop end resolves the old request; failure preserves text for Retry.

## W10. Open website or approved app

Screens: M12, M19, A07.

Parse an absolute HTTP(S) URL; reject malformed hosts, credentials, controls, unsupported schemes, and overlong values. Preserve valid path/query. Intranet addresses can be valid. The coordination server does not fetch the URL.

The endpoint requests launch through the actual user's normal browser association, without shell/elevation. Launch acceptance is not successful page loading. App launch resolves the approved platform catalog/revision and bounded typed arguments. Missing/changed entries fail; an operator cannot substitute a privileged executable. Offline targets never execute old launches on reconnect.

Attention and a pending machine-action reservation refuse new website/app launch. Do not let launch steal the protected interaction's focus or become delayed work after it ends.

## W11. Send files

Screens: M13, M19, E11. D14 confirms self-hosted server content storage with quotas and retention.

Use a native picker and platform-scoped source access; release handles when finished. Show names, bytes, destination category, and frozen targets. Inspect actual files without automatic archive extraction. Validate readable handles, file/count/byte limits, policy, recipient capability, and quota.

M13 names the confirmed D29 [file recipient scope](contracts.md#file-recipient-and-os-user-boundaries) before dispatch: For [User] on [Computer], with verified intended local identity and private destination. A target without a verifiable eligible user is Recipient Unavailable. Freeze the binding with the committed job; logout/user change pauses unfinished delivery as Waiting for Recipient until that same user returns before expiry. Revalidate current authority, identity, session generation, and ACLs at local commit, including changes during an upload. No shared-inbox or next-user fallback is allowed. A terminal expired/cancelled/revoked job cannot resume; replacement enrollment needs a new explicitly bound action.

Upload authenticated bounded chunks into isolated temporary storage. Verify actual bytes/hash before atomic publication. A changed source fails or uses a verified stable snapshot. Existing files use a safe new name or explicitly authorized replacement; no silent overwrite.

Report Uploading until server verification/atomic commit; Stored means durable server content, and Delivered means a recipient acknowledged its verified local commit. Server upload completion is not endpoint delivery. After Stored and committed job creation, delivery can continue when the sender console closes within the job's file-only authority/deadline. Closing before the local upload finishes shows Interrupted, never Stored. Cancel closes/revokes pending work and cleans temporary files/reservations; already delivered files remain identified.

Resume binds the same manifest/hash/target/current authority within a bounded retention window. Cancelled/expired transfers cannot revive. Test traversal, symbolic-link/reparse escape, path races, Windows reserved names/streams, Unicode/case collisions, quota reservation races, and interrupted commit.

The server owns content quotas, atomic reservation/commit, scoped retrieval, retention, deletion, temporary cleanup, and encrypted backup policy. File jobs cannot renew media/control or silently add later targets. Operator revocation, job expiry, and administrator cancellation revoke the appropriate job authority. Endpoint pickup can await availability within the explicit file deadline; power/input commands have no equivalent offline replay.

## W12. Request and receive files

Screens: M14, M15, E11. Server storage is confirmed D14; D21 confirms that closing refuses new submissions and retains completed files, while replacement requires explicit confirmation.

A file request has ID, owner, bounded title/instructions, eligible computer IDs, allowed manifest rules, close/deadline, and replacement policy. Save Draft sends nothing. Send Request publishes a validated revision. Computer users choose their own files; there is no operator home-directory browser.

Bind upload to request/current authorized submitting identity and revision. Submitted means durable verified commit of the complete file set, not buffered bytes. Retain Pending/Received/Rejected/Failed states and scoped download access.

Apply the [OS-user submission boundary](contracts.md#file-recipient-and-os-user-boundaries): a new local user cannot resume, replace, or retrieve another user's temporary/committed submission through the same computer ID. Keep request submissions separate from sent-file delivery. Attribution names a verified computer/local-user context unless a trustworthy product-person identity exists; no invented student account. V48 proves user-switch and commit races alongside the confirmed D21 close/replacement policy.

Closing is explicit or by configured deadline, independent of monitoring pause/console closure. This file-only authorization cannot renew observation/control. Closing refuses new submissions and retains completed files. Replacement while a request is open requires explicit confirmation; matching filenames never overwrite old work silently. Export stable IDs with safe labels to avoid same-name collisions.

Opening files is explicit, with no macro/executable preview, automatic archive extraction, or assumed malware safety. Report quarantine/scan status only when actually provided. Quotas/retention/deletion/backup/temporary cleanup are release requirements.

## W13. Screenshot

Screens: M16, M19.

Explicit screenshot authority is separate from live preview permission. Capture the selected current computer/display and use a native save picker. Never export a stale ended-desktop frame. Label verified source time or the actual known receipt/save time accurately.

Cancel leaves no new saved image/job. Save failure preserves safe destination context; Retry reacquires fresh authority/frame where needed. Batch capture, if offered, has explicit targets and collision-safe names. Screens do not enter ordinary server audit/support reports. Exported arbitrary files cannot be promised remotely erasable by the product.

## W14. Machine actions

Screens: M17, M19.

Wake uses authorized saved computer network identity and tested LAN egress; Wake Request Sent differs from Online. Authenticated reconnection proves online state, not sending a packet. VLAN/hardware limitations remain capability qualifications.

OS Lock, Logoff, Restart, and Shutdown are separate typed operations. Show immutable targets and unsaved-work consequences. Short execution deadlines reject unavailable computers; no surprise reconnect queue. Countdown/cancel requires an actual tested cancellation boundary; clicking Cancel alone does not prove cancellation.

Apply the [conflict matrix](contracts.md#interaction-conflicts) before dispatch and again at the helper's execution boundary. Name active observers and exclusive interaction owners in the consequence review. End observations and explicitly replace only owned or IT-stop-authorized conflicts; other operators' control/sharing/attention causes refusal otherwise. Reserve each target against new desktop claims, reconcile input/restriction teardown, then execute. A changed conflict set requires fresh review. Pending Release or an unknown previous machine action cannot be treated as safe execution permission. Wake does not stop interactions on an already online computer. V41 covers races and mixed target outcomes.

The helper rechecks caller, computer/session, authority, policy, typed operation, and deadline. No arbitrary shell/privileged executable. An OS effect may happen before durable acknowledgment; reconcile boot/session evidence or report Outcome Unknown. Never repeat reboot/shutdown automatically. OS Lock preserves native authentication; remote unlock/login is outside version 1.

## W15. Pause, sign out, close, or quit

Screens: M02, M20, M21, P02.

Pause Monitoring directly stops all this console's overview/detail observation contexts when viewing only. Active owned control has one clear consequence review and is stopped. Sharing/attention have independent authority and their own prominent stop controls; keep them visible while previews are paused. Sign-out/Quit lists and releases all owned active sensitive live work. Committed server file jobs/requests continue under separate file authority; pausing or closing a console cannot pretend an unfinished local upload committed.

Immediately stop local input and prevent renewal/admission for paused monitoring. Clear images/exportable caches. Reachable endpoints acknowledge release; others remain Pending Release until stopped/expired evidence. Pause never stops another operator's independent session.

Resume creates fresh monitoring authority. Reconnect while paused does not resume automatically. A new ordinary authenticated app launch follows D08 automatic previews. Closing an independent remote window stops its own access; closing only the main window does not silently end a still-open remote window. Quit/sign-out/server-switch releases owned authority. Crash/sleep/force termination uses leases rather than relying on close callbacks.

## W16. Accounts, computer access, policy, and diagnostics

Screens: A04 through A10, C03, P01 through P05.

Named local accounts, password/factor reset, administrator MFA, configurable operator MFA, recovery, session expiry, and rate limiting use the maintained E07 implementation. Separate configure/install/recover, observe/control/share, screenshot, files, and machine permissions. IT recovery does not imply viewing. Protect the last recoverable administrator and test the offline recovery route.

Follow the [administrative delegation/recovery table](contracts.md#administrative-delegation-and-account-recovery) for every editor/reset/approval/import. Manage Access checks resulting rights against explicit delegation scope/ceiling in the committing transaction. Manage Accounts cannot reset higher-authority targets or issue their normal sessions; commit invalidation before issuing bounded recovery-only material, then independently verify the target. Self/security changes use the separately scoped A09 review with fresh MFA. Protect the last recoverable security-authority account and prove direct/indirect/racing elevation denials through V46.

Computer access changes show exact IDs/capabilities and revocation impact. Using a group in this editor stages the current permitted computer IDs; later group edits do not silently grant access. New permissions do not start screens in an IT-only view. Account/factor reset invalidates the required sessions under the frozen policy.

Policy saves are transactional/versioned. Show Saved, Delivered, and Applied separately; offline endpoints are not marked updated. A user preference cannot weaken deployment denial. Import previews validated typed/schema changes; export omits secrets. Recovery codes display once, are verifier-stored/atomically consumed, and never logged.

Audit exports have authorized filters and metadata only. Support reports have a redaction preview. Native general/notification settings honor OS appearance/accessibility and essential control visibility. About links the corresponding source/version; Help uses task-based instructions, without classroom vocabulary.

## W17. Guided server setup

Screens: H01 through H12 for first-time setup. H13 is Deployment Verified. H14 through H19 are operation. E05/E07/E08 required.

Hosting availability depends on the manager app, not only the destination host. Optional containers are an advanced alternative.

| Shared Manager build | Available routes | H01 behavior |
| --- | --- | --- |
| Qt Manager on macOS | macOS local or Linux SSH | Show This Mac and Existing Linux Server; Windows local hosting is available from the Windows build |
| Qt Manager on Windows | Windows local or Linux SSH | Show This Computer and Existing Linux Server |

Windows local and macOS local follow H01 → H02 → H05. Linux SSH from either Manager build follows H01 → H03 → H04 → H05. All routes join at Check Host and follow H05 through H12. Windows hosting is installed locally by the Windows Manager build; remote Windows installation is unsupported. The local macOS route uses launchd and must pass the same readiness, ownership, update and recovery gates.

| Step | Route | Behavior |
| --- | --- | --- |
| Choose hosting | Platform-specific Qt Manager build | Show only the routes in the availability table; optional containers advanced |
| Local host | Windows or macOS local | This computer name, platform authorization, product install scope; no SSH fields |
| Connect | Linux SSH | SSH address and authentication |
| Host identity | Linux SSH | Verify SSH host identity before any write; changed key/refusal means no writes |
| Check | All, platform rows | Supported OS/architecture/privilege, disk/path/port/service conflicts, existing installation |
| Network | All | Explicit LAN or public listeners/ports/firewall scope; public access requires authenticated authority and bounded relay admission. No unexplained required 80/443 |
| Identity | All | Deliberately generate/restore deployment identity; export intended public details |
| Administrator | All | Named local account, real MFA/recovery setup |
| Storage | All | Namespaced server content paths, quotas/retention, encrypted backup destination, D14 stored files |
| Review | All | Exact product-owned files/services/listeners/firewall/storage/startup changes |
| Install | All | Phase journal, real progress, cancellation/rollback boundaries |
| Ready | All | Server Ready for Enrollment ends first-time setup: healthy service and enrollment can accept a profile |

Deployment Verified is separate later evidence on H13: one real enrolled authorized endpoint, authorized media, denial, teardown, and recovery. A TCP probe or service process alone cannot mark Deployment Verified. Finishing setup does not require a second machine. V39 records readiness in S01, authorized preview and denial evidence in S02, and completed deployment verification in S03; partial checkpoints cannot mark the deployment Verified.

Bootstrap administration is local-only or through authenticated SSH tunnel. Before Install, cancel leaves the host unchanged. Later cancellation follows the phase journal and cannot pretend a data migration is instantly reversible. Resume rechecks actual state.

H08/H11 follows the exact [initial security-account and manager binding](contracts.md#administrative-delegation-and-account-recovery), including MFA, single-use local/tunnel bootstrap, and the reviewed initiating manager identity. A first-administrator account or invited manager existing only in an untrusted backup cannot stand in for independent recovery authority. Storage preflight freezes reserves but later required commit failures still follow V49.

Dedicated namespaces/identities/path permissions/resource caps/log rotation prevent interference. No replacing host runtimes/global firewall/other RustDesk installation/unrelated services. Freeze real package/service identities and supported host distributions, not illustrative wizard copy.

## W18. Backup, restore, update, repair, and removal

Screens: H14 through H19.

Back up a consistent database plus selected config/keys/artifacts in a bounded encrypted package. Verify integrity, record what is included, keep recovery material separately, and test restore. A live unsafe database copy is not sufficient.

Restore checks integrity/compatibility/space/permissions/key handling, ends old authority, increments deployment generation, and reconciles post-backup revoked/new identities. Preserve a recoverable previous state. Clean-host continuity and compromise recovery are different tested procedures.

H16 follows the [missing-history restore contract](contracts.md#restore-with-missing-revocation-history). Show whether complete trusted post-backup history exists before activation. Without it, restore content in Recovery Quarantine and offer independent local/tunnel IT recovery: freshly validate accounts/MFA, reapprove managers, re-enroll endpoints, and explicitly rebuild computer access. Restored credentials cannot approve their own release. Release only the reconciled subset; cancelled/interrupted recovery remains quarantined. Missing independent recovery authority requires a new trust deployment, not a bypass. V44 proves old-identity denial and partial recovery.

H14/H16 also applies [active authority cutover](contracts.md#active-server-authority-and-cutover): drain and fence/isolate the old host, establish an independently approved fresh replacement issuer, and reconcile each peer's persisted trust. Show pending/offline/local-recovery peers separately; an old server returning later cannot regain authority over migrated peers. If old-host fencing is unavailable, stop the normal cutover and use compromise recovery without claiming inaccessible peers are globally revoked. V47 tests partial transitions, cancellation, concurrent copies, and retired-host return.

Updates use signed compatible artifacts tied to exact source pins, architecture, ABI/protocol range, and schema. Offline import keeps internet optional. Drain/defer owned active work, pilot rollout, report unavailable endpoints, and pause at the frozen failure threshold. Rollback requires actual code/data compatibility; old code cannot start against an incompatible new database.

H17 follows the [security-floor and rollback contract](contracts.md#signed-updates-and-authorized-rollback). Preflight checks security revision and trusted metadata freshness/version as well as signature/compatibility. Below-floor releases are Refused, including offline imports and correctly signed old installers. Rollback requires separately authenticated scoped IT intent, exact artifact/targets, fresh trusted metadata, compatible data, and unchanged security floor. No generic downgrade checkbox or automatic rollback to a vulnerable release. Lost floor state enters recovery rather than resetting to an old backup's value. V45 covers install, repair, update, offline import, restore, and safe authorized rollback.

Repair compares actual resources to the product manifest and preserves data. Remove defaults to keeping data/backups and removes only owned resources. Data deletion is separate explicit intent. Authorized endpoint emergency recovery works without the coordination server.

H14/H18 follows [storage recovery](contracts.md#failed-persistence-and-storage-recovery): show failed commits/revisions and authority readiness, preserve teardown, perform scoped repair/integrity/checkpoint/pending-action reconciliation, then prove a harmless durable transaction before normal admission. Never auto-reset data/trust or silently replay side effects after space returns.

## Readiness boundary

Helicoid naming, D18 inventory/inclusion behavior, and D21 file-request closure/replacement are confirmed on 10 October 2026. Remaining engineering prerequisites and visible draft rules still need proof before dependent slices are Ready. D14 server storage, D19 multiple viewers/one controller, D20 manager approval, D26 console-only sessions, D27 enabled local input, D28 Mac manager icon, D29 queued delivery waiting for the intended user, and D30 owner/IT presentation stop are confirmed. E01 through E08 require real outputs, including V40–V50 for desktop/security/recovery boundaries and V51 for native icon integration. No classroom join/start/end workflow is part of the implementation.
