# System contracts

Draft updated on 10 October 2026 for the general computer-monitoring model. [Decisions](decisions.md) distinguish confirmed behavior from proposals. Concrete schemas, headers, cryptographic formats, and measured limits must be frozen before callers are implemented.

## Invariants

| ID | Required invariant | Enforcement |
| --- | --- | --- |
| I01 | Automatic previews still require authenticated per-computer observation admission before pixels | Manager, server, endpoint, caches; opening authorized console is the D08 trigger |
| I02 | Account, organization, computer, manager, monitoring session, interaction, and OS desktop are distinct identities | Model, scoped queries, protocol, grants |
| I03 | Every operation uses authenticated authority and explicit computer IDs | Server and executing endpoint/helper |
| I04 | One action's targets are immutable | Stored target snapshot; filtering and later enrollment cannot expand it |
| I05 | One active controller owns a computer at a time | Atomic server and endpoint control claim |
| I06 | Operators cannot silently displace another operator's control, attention, or sharing | Per-interaction ownership and explicit authorized recovery |
| I07 | The managed computer user has no product command to end operator control | Endpoint command surface and authorization |
| I08 | Owner/authorized IT can stop control; stop authority does not grant viewing | Separate teardown/recovery capability |
| I09 | Expiry, revocation, desktop change, permission loss, or relevant process failure invalidates access | Server, core, worker, helper, native UI |
| I10 | Reconnect never replays input, expired work, or an offline shutdown | Bounded queue semantics and deadlines |
| I11 | Issued, delivered, applied, unavailable, and outcome unknown are different | Protocol, action journal, UI |
| I12 | Duplicate requests cannot silently repeat a side effect | Stable action ID and endpoint deduplication where applicable |
| I13 | Admission, queues, storage, retention, and relay traffic are bounded | Server/endpoint quotas and typed overload results |
| I14 | Privileged code accepts typed operations, never arbitrary shell | Helper interface, approved catalog validation |
| I15 | Default logs/backups do not contain transient screens, credentials, message bodies, or clipboard data | Audit, diagnostics, caching, backup selection |
| I16 | The computer user can see active observation/control identity | Worker and monitored-computer status UI |
| I17 | Updates preserve trust and permission enforcement | Signed compatibility manifest, migrations, downgrade policy |
| I18 | Implementation repositories build independently using pinned contracts/core | Build metadata and release manifest |
| I19 | No operation requires a room, class, seat, course, lesson, or participant membership record | Model, navigation, protocol, authorization |
| I20 | A desktop operation resolves one eligible OS session deterministically; it cannot fall back to another signed-in user | Trusted local session resolver and generation-bound admission |
| I21 | A competing enrolled identity is quarantined before new authority is issued | Atomic incarnation/connection claims, endpoint fencing, IT replacement |
| I22 | Restoring without trustworthy later revocation history cannot reactivate historical access | Recovery quarantine, independent IT validation, fresh authority |
| I23 | A valid artifact signature cannot bypass the minimum accepted security revision | Installer/updater/recovery checks and persistent release policy |
| I24 | Administrative rights have explicit delegation and recovery limits; an IT-only account cannot silently obtain observation through account/access management | Server-owned grant ceilings, protected security review, recovery-only credentials |
| I25 | Each peer admits one explicitly trusted active deployment authority; a copied/restored server does not automatically replace it | Authority lifecycle, independently authorized cutover, endpoint trust checkpoints |
| I26 | Sent files belong to the verified intended user on the enrolled computer; logout/user change pauses unfinished delivery and cannot change its recipient | Immutable user/computer binding and private destination, endpoint commit checks |
| I27 | Failure to commit required security/action state prevents new authority and side effects, while safe teardown stays available | Durable admission/dispatch boundaries, degraded mode, independent cleanup |
| I28 | Only the sharing owner or scoped authorized IT can explicitly stop a receiving presentation; automatic safety teardown remains effective and branch stop cannot affect other recipients or restart itself | Endpoint-enforced stop authority, generation-bound teardown, distribution results |

Optional simple groups organize existing computer IDs without a mandatory hierarchy. A group name cannot grant permission. Selecting All Computers means the account-visible inventory, never every discovered network address.

Monitoring inclusion, displayed filtered IDs, temporary action selection, immutable action snapshots, and admitted stream demand are distinct. All/explicit-ID inclusion is an account/deployment preference. New actions use selected displayed IDs or, with no selection, all displayed IDs. Changes to inclusion/search/status clear tile selection. No preference creates authority or retargets an open action. A zero-sized target set cannot dispatch. See [native selection rules](../ui.md#veyon-workflow-adapted-to-the-native-apps).

An engine that cannot enforce I01, I03, I05, I09, or I16 at every admission path fails E01. An unrestricted upstream listener behind a native UI is not a valid implementation.

Under D25, an overview preview uses the endpoint-reported primary display of its current desktop/layout generation. Display identity is not a hard-coded index. Primary-display changes invalidate the old source/layout mapping and require current authorized source admission. Missing/unavailable primary display produces a named readiness state; do not silently substitute another display or combine all displays. Detailed windows expose their own display picker.

## Logical records

IDs are opaque, immutable, and separate from display names/addresses. This is a logical model; the reviewed migration and schema are E02/E03 outputs.

| Record | Required logical fields | Constraints |
| --- | --- | --- |
| Organization/deployment | id, displayName, authorizationGeneration, policyRevision | One owner/deployment in version 1; no school-specific hierarchy |
| Account | id, organizationId, loginIdentity, displayName, status | Local operator/admin identity; credential material handled by selected maintained auth implementation |
| Access assignment | accountId, organizationId, capabilities, computerScope, revision | Server-owned computer scope; caller role labels are not authority |
| Administrative assignment | accountId, permissionFamily, managedAccountScope, managedComputerScope, delegationCeiling, revision | Separate from operation permissions; granting scope cannot exceed the authenticated actor's explicit delegation authority |
| Computer | id, organizationId, publicIdentity, approvedIncarnationIdentity, enrollmentEpoch, friendlyName, platform, enrollmentState, profileRevision | Unique enrolled endpoint; quarantined/replaced keys cannot admit operations; discovery is not enrollment |
| Manager identity | id, organizationId, publicIdentity, lifecycleState, approvedBy, approvedAt | Separate from account; one-time administrator approval required; pending/revoked identities fail admission |
| Desktop session | computerId, desktopSessionId, actual OS session/user, sessionType, eligibilityReason, boot/workerGeneration, desktopGeneration, capabilityRevision | Trusted local selection under D26; requester cannot choose another user's session |
| Endpoint connection claim | computerId, incarnationIdentity, bootGeneration, connectionGeneration, channelBinding, state, expiry | One admitted incarnation; authenticated reconnect fences the old channel; no address-based identity |
| Capability snapshot | computerId, desktopSessionId, revision, operation support/availability/permission/reason | Online, logged in, screen ready, and control permitted are separate |
| Monitoring session | id, organizationId, ownerAccountId, ownerManagerId, intent, computerIds, scopeRevision, state, maximumEndAt | Operator-owned overview/detail context; no room or lesson reference |
| Observation target | monitoringId, computerId, desktopSessionId, state, interactionId | Connection and admitted desktop authority are distinct |
| Interaction | id, ownerAccountId/managerId, optional monitoringId, source/targetIds, kind, generation, state, expiry | Observation/control/sharing/attention have separate lifecycles and ownership |
| Action | id, actor, operation, immutableTargetIds, deadline, payloadReference, state | Optional monitoring context; file/power/catalog actions do not require a lesson |
| Target result | actionId, computerId, desktopSessionId if applicable, state, reasonCode, evidenceTime, endpointGeneration | Old desktop results cannot update new desktop state |
| Enrollment request | id, devicePublicIdentity, invitationId, state, expiry, role | Atomic invitation use/approval; no room hint or seat assignment |
| Policy revision | revision, scope, typedValues, previousRevision, activationState | Validate before activation; permissions combine restrictively |
| File request/artifact | id, owner, eligibleComputerIds, title, objectManifest, close/retentionPolicy, state | D14 server content storage with quotas/retention; filenames are untrusted display data |
| File delivery recipient | actionId, computerId/enrollment binding, required trusted intended local user binding, private destinationCategory, state, fixed deadline | D29 user/computer intent is mandatory; filenames, friendly labels, or whichever user signs in next cannot choose the recipient |
| Audit event | id, actor, action, targetIds, time, result, reasonCode | Metadata only by default |
| Optional organization group | id, displayName, computerIds, revision, owner/visibility | Flat sets; multiple membership; no authority attached to a name |
| Overview preference | accountId, organizationId, inclusionMode, includedComputerIds, panelVisible, view, previewSize, sort | Revalidated UI preference; All or explicit IDs; no grants, frames, or persisted control state |
| Recovery reconciliation | recoveryId, newDeploymentGeneration, historyWatermark, trustedEvidence, reviewedPrincipalIds, state | Restored roles/keys remain quarantined until their reconciliation is complete |
| Authority transition | transitionId, previousAuthorityIdentity, replacementAuthorityIdentity, lifecycleState, independentlyVerifiedIntent, peerReconciliation | Restoring copied credentials alone cannot approve cutover; peer acknowledgement and old-host isolation are separate evidence |
| Persistence health | state, failedBoundary, lastTrustedRevision, recoveryEvidence | Required commit/read failures latch admission failure; an in-memory success cannot manufacture a saved revision |
| Release policy | component/platform, minimumSecurityRevision, highestAcceptedReleaseSequence, trustedMetadataVersions, policyRevision, evidence | Nondecreasing trusted floors/checkpoints; backup/import/reinstall cannot silently lower them |

Computer-user identity is optional. A trusted name can be secondary UI information, but an OS account label does not manufacture an authenticated product account. Computer name is the default primary label.

## Administrative delegation and account recovery

This is the draft E02/E07 permission design. Role names are convenient presets only; the server checks explicit administrative capabilities, managed account/computer scope, and a delegation ceiling separately from the actor's own operation permissions. Having permission to perform an operation does not grant permission to delegate it; having a bounded delegation right does not itself grant pixels or input.

| Permission family | Allowed authority | Forbidden inference or escalation |
| --- | --- | --- |
| Operate computers | Execute individually admitted operations on explicit computer IDs | Cannot grant access, approve managers, or reset accounts |
| Install, maintain, recover, or stop interactions | Reviewed owned resources and scoped teardown/recovery | Cannot subscribe to screens, edit grants, or produce a normal login as another account |
| Manage enrollment | Approve/revoke exact endpoint/manager identities in the assigned scope | Cannot add observation grants; a pending manager cannot approve itself |
| Manage access | Grant/revoke only capabilities and computer IDs within the explicit delegation ceiling | Cannot widen its own ceiling, edit a protected security account, or use a role label/group/import to escape the ceiling |
| Manage accounts | Create/disable or initiate recovery for accounts in the assigned account scope and allowed privilege ceiling | Cannot reset higher-authority accounts, mint normal sessions as the target, or disclose target factors/recovery codes |
| Manage security authority | Explicitly review administrative assignments, protected accounts, delegation ceilings, and exceptional trust/recovery changes | Requires fresh MFA and a consequence review; it is an intentionally trusted authority, not a hidden privilege attached to every IT account |

Scope and ceilings apply to the resulting state, including indirect grants, alternate accounts, preset edits, configuration imports, and concurrent changes. The server validates the actor's current revision and target revision in the transaction that commits the change. Refuse attempts outside either ceiling. Ordinary access editing cannot alter the actor's own grants or delegation; route a permitted self-change through the separately authorized security review, show the exact effective rights gained/lost, and audit it. Creating another account does not bypass the same resulting-rights limits.

The initial named administrator receives the explicitly reviewed security-authority assignment during authenticated local/SSH-tunnel setup, after MFA/recovery setup. Its bootstrap is single-use and transactional. It may bind the exact initiating manager identity only through that verified bootstrap channel; no other pending manager is auto-approved. Later approval uses an already approved authorized manager with fresh authentication or the independent local recovery procedure. A security administrator may approve their own new manager from an already approved context; the new pending manager alone cannot authorize that approval. First-time setup still ends at Server Ready for Enrollment without an endpoint, and an administrative view never starts observation just because the account can manage security.

Credential reset first invalidates the target's affected sessions and factors under the frozen E07 policy. If invalidation cannot commit, do not issue reset material or report Reset. Recovery material is bounded, one-time, scoped to credential recovery, and cannot authenticate a normal computer-operation session. Deliver it through a verified target recovery channel; an ordinary resetting administrator cannot retrieve/use it as the target. Completion requires independent target verification using a surviving approved factor/context, or the separately authorized security/local recovery procedure when those are lost. Losing both factors requires that stronger procedure, not a temporary password chosen by an IT operator. Newly established factors/codes are shown only to the verified recovering account. Account recovery does not grant manager approval, new access assignments, or pixels.

Protect the last recoverable security administrator and its independent recovery route when disabling accounts or changing ceilings. A deliberately trusted security administrator can authorize explicit new screen grants, including a reviewed self-grant; the product must not claim that such an authority, or a host administrator able to replace binaries, is cryptographically incapable of obtaining access. The enforced separation is that ordinary installation, account, enrollment, or stop-only rights do not silently imply that authority.

Primary reference checked on 10 October 2026: [OWASP Authorization Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Authorization_Cheat_Sheet.html) supports explicit least privilege, default denial, and checking each request. The table and recovery policy above are HS design requirements; they are not proof of a selected authentication library. V46 must exercise direct and indirect elevation, self-approval, bootstrap, reset, import, and concurrent revision changes before this boundary is Ready.

## Active server authority and cutover

Version 1 has one active authority per deployment and no active-active server/failover promise. Presence, routing locators, an address change, copied database, or possession of an old backup do not select the current authority. E03/E07 must freeze the actual authority identities, generation/checkpoint representation, authenticated transition proof, and protected persistence; these logical states are not an invented signed protocol.

| Authority state | Permitted behavior | Admission rule |
| --- | --- | --- |
| Preparing or restored in quarantine | Verify content, history, independent recovery authority, and proposed replacement identity | No ordinary grants, enrollment approvals, file pickup, or policy activation |
| Active | Serve committed current policy for peers that trust this exact active binding | Normal scoped bounded authority; endpoint control fencing still applies |
| Draining | Stop new admissions, renewals, approvals, and mutations; reconcile existing live work | Teardown/recovery remains available; no automatic failover |
| Awaiting Cutover | Independent IT review establishes replacement identity and old-host fencing evidence | No normal replacement-server authority yet |
| Retired | Old authority/resources retained only as a protected recovery/archive source | No accepted new grants or return to Active from a retry/restart |
| Authority Needs Review | Conflicting authenticated bindings, lost trusted checkpoint, or incomplete cutover evidence | Deny affected new authority and guide independent recovery |

Normal migration drains the old authority, stops/isolates its grant and routing services using a reviewed product/host procedure, verifies a consistent transfer, and generates a fresh replacement authority identity. Independent security/local recovery authority approves an exact transition from the pinned predecessor to that replacement; a credential existing only in the transferred backup cannot approve itself. Increment the authorization generation and retire the old issuing identity. Do not reuse the old private grant-issuing key to make migration look like a network reconnect. Preserve computer IDs only after reviewed reconciliation; access/history still follow the restore contract.

Managers/endpoints validate the transition through their existing independently trusted recovery mechanism or an explicit local verified profile. Persist the new authority binding/checkpoint before accepting its operations, reject the retired issuer thereafter, clear old grants/frames/input/restrictions, and reacquire view-only authority. A numerically larger generation signed only by the retired issuer is not a valid transition. Cancellation before cutover leaves the verified prior installation available; after any peer adopts the replacement, never reactivate the old authority automatically. Recovery requires a new reviewed transition rather than decrementing checkpoints.

Track peer results separately: Trust Updated, Pending Trust Update, Needs Local Recovery, or Failed. An offline peer cannot be called migrated or safely fenced merely because the server database changed. Keep it unavailable in the replacement deployment until verified reconciliation. Bringing the original server back later must fail against already migrated peers. Detectable competing authenticated authority channels/bindings enter Authority Needs Review; first/last connection wins and silent pin replacement are forbidden. A routine address change for the same healthy authority needs authenticated locator reconciliation, not a new trust identity or weakened key check.

There is a hard limit: isolated peers that still trust a copied old signing identity cannot learn that it was retired without trusted transition information. Software cannot universally distinguish two servers with identical copied secrets or remotely fence an unreachable clone. Cutover completion therefore requires actual old-host shutdown/isolation plus peer reconciliation evidence; if old-host fencing cannot be established, use compromise recovery, fresh identities, and local re-enrollment. Do not claim globally revoked access on unverified peers. Version 1 does not solve this by adding an unreviewed distributed consensus service. V47 must cover concurrent old/restored servers, partitions, offline catch-up, return of the retired host, partial/cancelled transitions, and the documented limit.

The server's schema 20 persists Active/Draining/Awaiting Cutover and bounded metadata-only audit events with configurable retention. Audit reads require a fresh-MFA security-admin Manager session; implemented account, access, group, enrollment, Manager, observation, interaction, file, recovery, cutover, bootstrap, self-service MFA enrollment, and MFA-policy event writes commit atomically with their changes. Denied-attempt coverage, remaining action paths, and full audit acceptance remain open. The server publishes the durable authority generation with the deployment ID and grant-signing key, and requires a fresh-MFA security-admin review of a named fencing procedure and evidence reference before releasing restore quarantine. The old-host evidence is an operator attestation; the service does not independently validate the referenced host action. The Manager stores a locally verified ID/key/generation tuple, rejects generation rollback and same-generation key substitution, and rechecks it before protected identity flows. Endpoint enrollment records version 4 store the same checkpoint; macOS and Windows file/session paths recheck the live authority and stop when it differs, requiring local re-enrollment after recovery review. These local guards do not report peer adoption, validate two-host fencing, or complete V47.

The shared Qt Manager Server workspace now has source-level status, drain,
cancel, replacement-approval, and recovery-quarantine release controls for
these v2 routes. It requires the saved server identity to match before opening
the workflow and relies on the server's fresh-MFA, exact-identity, and recovery
count checks for writes. Its local pin includes the authority generation and
all guarded flows reject a changed checkpoint. The Manager still has no
per-peer adoption results, and the Qt target has not been built in the current
environment. Mac endpoint source persists and checks the checkpoint; Windows
source has matching changes but is not buildable in the current environment.

## OS session selection

D26 confirms physical-console monitoring only in version 1, with no operator session picker. The computer ID represents the machine, not every signed-in user. Adding RDP support requires a separately specified selection and disclosure contract.

| Local state | Selected session | Result |
| --- | --- | --- |
| Windows has an unlocked, signed-in physical console user | Exactly the session attached to the physical console, with an authenticated matching user worker | Eligible for independently permitted capture/input |
| Windows has console and RDP users | Physical console only; RDP users do not compete for selection | Other sessions are unsupported; never choose the first Active session |
| Windows has only an RDP user or a disconnected/background user | None | Unsupported Session or No Console User, without another user's pixels |
| Mac has an unlocked, signed-in primary console user | Exactly that user's current GUI session and authenticated worker | Eligible subject to actual capture/input permissions |
| Mac has fast-user-switched background users or another virtual GUI session | Primary console user only | No background/virtual-session fallback |
| No console user, session attachment transition, locked/login/protected desktop, or ambiguous OS facts | None | No User Signed In, Session Changing, Locked, or Unsupported Session as applicable |

The service independently validates OS session/user identity, worker ownership, and eligibility. Neither a process claiming to be active nor a manager-supplied session number selects the target. Session 0, login windows, and protected desktops are not ordinary user desktops. Selection, user, lock state, boot, or worker replacement invalidates old authority before capture/input on a replacement; even reuse of an OS session number does not reuse desktopGeneration. A safe return can regain view-only after fresh admission, never control.

Primary-source API notes checked on 10 October 2026: Microsoft's [WTSGetActiveConsoleSessionId](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-wtsgetactiveconsolesessionid) identifies the physical console and can report no attached session. Apple's [SCDynamicStoreCopyConsoleUser](https://developer.apple.com/documentation/systemconfiguration/scdynamicstorecopyconsoleuser%28_%3A_%3A_%3A%29) reports the primary console rather than users switched into the background. These are selection inputs, not proof of unlocked state, capture permission, or worker authenticity. E04 must prove those checks and race handling on installed systems.

## Enrollment identity competition

Generate an installation incarnation identity in the selected protected local store after imaging. Bind it to the approved endpoint key and enrollment epoch. Do not put private endpoint/incarnation identities or resumable channel secrets in reusable images. Hostnames, IP/MAC addresses, and hardware hints are diagnostics, not proof that two connections belong to one installation.

| Authenticated event | Required transition |
| --- | --- |
| Approved key and incarnation reconnect with valid current resumption proof | Atomically fence the old connection generation before admitting the new channel; old callbacks/grants fail |
| No live connection claim exists; approved key/incarnation authenticates a fresh boot/channel in the current epoch | Admit a new bounded connection claim after normal policy checks; obtain fresh desktop authority, never resume old input |
| Enrolled key presents a different, unapproved incarnation | Quarantine the enrolled identity and both competing claimants; do not silently update its binding |
| Two independent live claims compete without valid channel resumption | Quarantine, including same-incarnation competition; first/last connection wins is forbidden |
| A claimant has no valid key-possession proof | Reject and rate-limit; an unauthenticated collision report cannot quarantine a real computer |
| An identity is already quarantined, revoked, or replaced | Deny new grants and renewals regardless of its former inventory/access membership |

Quarantine increments the enrollment epoch, fences connections, clears affected media/caches, releases input and restrictions through bounded teardown, and stops affected file/action authority. Keep one inventory record with Identity Needs Review, bounded security evidence, and no pixels. A network retry, changed name/address, or elapsed time cannot clear quarantine. Server and endpoint enforce the state; UI hiding alone is insufficient.

Resumption cannot silently displace an independently live channel merely because the new claimant has copied persistent credentials. While the old claim is valid, require handoff acknowledged through its authenticated channel or reconcile its fenced closure/expiry before new admission. A temporarily unreachable old channel yields bounded Pending Reconnect with no new operation authority, not automatic clone quarantine. Independently authenticated simultaneous claims without valid handoff are competition. E07 freezes the proof/handshake and E06 its bounds; a caller's assertion that the old channel closed is not evidence.

IT replacement requires scoped enrollment-recovery authority and fresh authentication. Identify the intended physical machine through a trusted local/managed-deployment process; retire the copied key permanently; generate fresh endpoint/incarnation identities; and approve their explicit association with the intended computer record. Show the exact proposed access assignments for review before activation. Preserve the computer ID only for a verified replacement of that machine; a second real machine gets a new record and explicit access. The rejected clone remains blocked. Replaced manager identities also require D20 approval again.

Detection is limited: two sequential copies containing identical keys, incarnation data, and resumption secrets may be indistinguishable without additional trusted hardware evidence. Version 1 must not claim universal clone detection or hardware attestation. E07 must prove the competing-claim and reconnect rules, document this limit, and never use fingerprints to manufacture certainty.

## Authorization and admission

Use maintained reviewed implementations. These are semantic checks, not an invented cryptographic envelope.

1. Authenticate the local account and bind it to the actual once-approved manager public identity. Check manager approval/revocation, account status, deployment, role/access revision, and authorization generation.
2. Resolve requested computer IDs through current server-owned access assignments. Check the exact operation capability. No room/class check exists.
3. For desktop actions, resolve the target's actual current desktop and capabilities. Machine-level actions have explicit rules when no desktop is signed in.
4. For observation/control, validate the operator's active monitoring scope. For independently authorized actions, use their action/interaction scope without fabricating a lesson.
5. Issue authority bound to deployment, account, manager key, target key, current desktop when applicable, operation, interaction/action identity, revision, expiry, and replay/channel context.
6. The endpoint independently checks those bindings and peer key possession before capture/input/files/privileged work. The privileged helper also checks its actual local caller/session/code identity.
7. Maintain per-operation ownership and lease state. Revocation tears down affected channels. Missing a revocation packet does not turn bounded authority into indefinite authority.
8. Return typed admission results. A refusal never falls back to a guessed upstream ID, permanent password, public relay, or weaker authentication route.

A scoped IT stop capability tears down an identified interaction without opening its media. An owner can request teardown of their own interaction even as their earlier start permission is being revoked. The managed user cannot impersonate either caller.

Multiple authorized observation sessions coexist independently under D19. Only one controller may exist per computer. Stop Monitoring ends the requesting operator's monitoring authority; it is not an organization-wide stop command. Observer identity lists must reflect admitted sessions, not logged-in accounts or mere tile inclusion.

## Interaction conflicts

This is the draft E02/E03 arbitration contract for one selected desktop. Columns are existing state; rows are requested state. Apply every occupied column, including Requested/Stopping/Releasing states until their claims are safely ended. Machine actions here mean OS Lock, Logoff, Restart, and Shutdown; Wake is covered separately below.

| Request | Observation only | Control | Sharing source | Sharing receiver | Attention | Machine action pending |
| --- | --- | --- | --- | --- | --- | --- |
| Observe | Coexist | Coexist | Coexist | Coexist | Coexist | Refuse |
| Take Control | Allow | Reuse exact owned interaction or refuse | Coexist | Replace | Replace | Refuse |
| Share a source | Coexist | Coexist | Reuse exact owned interaction or replace | Replace | Replace | Refuse |
| Receive a share | Coexist | Replace | Replace | Reuse exact owned interaction or replace | Replace | Refuse |
| Apply Attention | Coexist | Replace | Replace | Replace | Reuse exact owned interaction or replace | Refuse |
| Machine action | Review and end observations | Replace | Replace | Replace | Replace | Return same action result or refuse |

Coexist/Allow still require each operation's own permission and disclosure. Observation while receiving/sharing/attention sees only the admitted desktop, not a hidden alternate surface. Sharing a source may coexist with control because capture/distribution and input have independently admitted scopes; it does not create another controller. Different source shares or receiver overlays do not silently stack in version 1.

Replace requires a consequence review naming exact interaction IDs/owners and affected computers/recipients. An operator can replace only their own interactions. Replacing another operator's interaction requires an explicit scoped IT stop capability as well as permission for the new operation; otherwise refuse with In Use and a Contact Operator/IT route. This never grants access to the stopped interaction's pixels. Receiving/source reversal, control after attention, and sharing after attention use the same rule as attention after those actions.

Ownership uses the authenticated account/manager binding, never a matching display name or a caller-supplied owner field.

Reserve the transition atomically against the expected interaction set/revision and fence it at the endpoint. If a new conflict appears, refuse/review again; do not execute the stale confirmation. Stop conflicting claims, reconcile endpoint teardown and held-input release, then admit the replacement. Pending Release is not completion. Cancel before teardown preserves prior activities; cancel/failure after teardown leaves them ended and never restarts them automatically. Results are per computer, not atomic across the fleet.

Ending a sharing receiver branch clears only that recipient and records it ended in the original immutable target set. Ending the sharing source clears the entire distribution; review must name that larger consequence. Other authorized observers continue for ordinary replacement. Machine-action review includes all observers and ends desktop-bound observation before execution, without requiring pixel access or displacing foreign exclusive interactions without stop authority. While the machine action is reserved, reject new desktop claims; clear the reservation only on a reconciled terminal result/expiry. Unknown outcome never triggers automatic repeat.

Wake of an already online computer is a no-op, not an interaction replacement. Messages/help and server file jobs coexist without focus theft or renewed media authority; power/session loss pauses or fails endpoint-dependent work honestly. Website/app launch is refused during Attention or a pending machine action. Screenshots require separate current-desktop permission and cannot bypass protected/unsupported state.

## Bounded lifetime and clocks

Initial candidates: media/control authority at most 30 seconds without renewal; attention at most 60 seconds. These are engineering targets awaiting E06 measurement, not timing guarantees.

E03/E07 must freeze issuance/expiry representation, admission skew, replay handling, and conversion to monotonic local deadlines. Wall-clock changes, duplicate grants, and reconnects cannot extend an existing lease. A restarted worker needs fresh authority when it cannot prove remaining validity.

On expiry/revocation: stop input, release held keys/buttons, stop capture/publication, invalidate exportable frame caches, clear receiver images, and release the affected temporary restrictions. Preserve OS authentication. No new operation runs until fresh authorization.

## State transitions

### Monitoring session

| Current | Event and precondition | Result |
| --- | --- | --- |
| None | D08 start trigger; authenticated computer scope validated | Starting; stable request identity and computer snapshot; no pixels yet |
| Starting | Session committed and target admissions started | Active; each target reports Connecting/Live/Unavailable independently |
| Starting | Authentication/scope/session creation fails | Stopped with failure; unwind partial authority |
| Active | Existing target reconnects | Fresh current-desktop admission; no input replay |
| Active | Current inventory/inclusion updates overview scope under final D18 policy | New scope revision; new targets need fresh grants; existing action snapshots stay fixed |
| Active | Stop Monitoring, sign-out, account revocation, or unrecoverable authority loss | Stopping; deny renewals/new admissions and tear down this owner's related access |
| Stopping | Reachable targets stop | Stopped; unreachable targets retain Pending Release/expiry evidence |
| Stopped | Start again | New session identity, no old grants or queues |

New enrollment never expands an action snapshot. The D18 draft permits fresh view admission for a newly eligible inventory computer under current access. Observation-only reconnect can resume after fresh admission; control never resumes automatically. Pause persists across transient reconnect/group/search changes.

A detailed window obtains its own operator/manager-bound detail monitoring context, separate from the overview's included-ID set. Unchecking a tile ends its overview observation, not that detail context. Search/status changes update overview demand and selection without granting authority. Pause Monitoring stops all overview/detail observation and control contexts belonging to this console. Screen sharing, attention, and file jobs have independent interaction authority; pause alone does not revoke them. Sign-out/Quit/server-switch ends owned live media/control/restrictions, while committed server file jobs follow their separate file authority.

### Interactions

| Kind | States | Exit rule |
| --- | --- | --- |
| Observation | Requested, Connecting, Live, Stalled, Ending, Ended, Failed | Close ends its stream; authority/session change clears frames |
| Assistance | Requested, Authorizing, Controlling, Stopping, Stopped, Failed | Unique controller; explicit Take Control; release held input on all exits |
| Screen sharing | Preparing, Sharing, Stopping, Stopped, Failed | Explicit source/recipients; source loss clears receiver content |
| Attention | Requested, Applying, Applied, Releasing, Released, Expired, Failed | Per-computer acknowledgment; independent watchdog/recovery |

Each interaction has a generation. Late frames/callbacks/acks cannot resurrect a stopped interaction. Discarded buffers still need disposal. Reopening a remote window creates fresh observation, not an implicit return to control.

Under D24, minimizing a detail window immediately stops local input forwarding, releases held keys/buttons, and cancels pending control admission or starts release of active control. Its separately authorized observation stays active. Restoring the window never restores control; Take Control is explicit and cannot start while the previous claim's release is unresolved. A late control acknowledgment cannot reactivate the minimized/restored window. Ordinary focus loss stops forwarding and releases held input without itself implying minimization.

### Local and remote input ownership

Forward input only when the admitted controlling window is foreground/key, its remote canvas has explicit focus, no local menu/dialog/sheet/popover is active, and the current interaction/desktop/display generations still match. A toolbar click, native picker, confirmation, text field, menu shortcut, or another application always receives local input. Reserved OS/app shortcuts and Stop Control remain local; enumerate exact Mac/Windows shortcut mappings through E04 before implementation. The shortcut used to focus the canvas is consumed locally rather than injected as a remote click/key.

On canvas focus loss, menu/dialog opening, pointer ownership leaving an active remote drag, or application/window focus loss: stop forwarding immediately, release tracked injected keys/buttons through the control channel, discard pending text/IME composition, and fence late events. Moving to local UI does not end the controller lease by itself. Returning requires explicit canvas focus and still-valid control; minimize follows the stronger D24 release rule. No local typing is replayed later.

D27 keeps the computer user's physical keyboard/mouse enabled during control; remote and local input may interact. Local input is neither suppressed nor recorded and has no product stop-control capability. Ordinary OS locking/user switching still triggers automatic security teardown. Version 1 has no block-input toggle. Any later suppression capability requires its own policy, disclosure, permission, watchdog, reserved-OS behavior, and installed-platform proof. E04 must test overlapping physical/injected chords and cleanup; track injected input without treating every locally held key as a remote key to release.

Stale screens display honest receiver update age. Do not label receipt time as source capture time without a timing proof. Clear pixels after the configured threshold and after authority ends; hiding an image while retaining an exportable unauthorized cache is insufficient.

### Action results

| State | Meaning | Retry |
| --- | --- | --- |
| Queued locally | Undispatched current user intent | Cancel or dispatch before deadline; no reconnect action queue |
| Issued | Server durably accepted scope/action | Query same action ID |
| Delivered | Authenticated endpoint accepted message | Await effect result |
| Applied | Endpoint reported the defined effect | Return recorded result for safe duplicate ID |
| Rejected | Authority/capability/policy/input denied | Correct cause; explicit new intent can use new ID |
| Unavailable | Target offline/session absent before execution | No deferred shutdown/launch for reconnect |
| Cancelled | Cancellable work stopped before its stated boundary | Explicit retry only |
| Expired | Deadline/authority ended without verified effect | Fresh intent/authority, no replay |
| Outcome unknown | Effect may have happened but durable ack is missing | Reconcile before repeating destructive work |

Deduplication binds action ID, operation, target, desktop generation when applicable, and validated payload digest. Same ID with different payload is rejected. A crash between an OS side effect and durable recording can still produce Outcome Unknown; universal exactly-once execution is not promised.

## Typed command catalog

Every command carries version, stable request/action identity, authenticated origin, explicit target IDs, authority context, deadline, and bounded typed payload. Receiver-derived roles replace caller-supplied permission labels.

| Family | Payload | Result and restrictions |
| --- | --- | --- |
| Start/Stop Monitoring | Authorized computer snapshot or owned session ID | Automatic console opening starts admission; Pause/Resume controls this owner's monitoring; no room/class/lesson field |
| Refresh Monitoring Scope | Existing session, explicit new computer IDs, scope revision | Validate each addition/removal; never expand old action snapshots |
| Start/Stop Observation | Current computer desktop/display, preview/detail intent | Interaction state; no implicit control |
| Start/Stop Control | Current desktop and owned interaction | Unique controller lease; stop rights separate |
| Remote Input | Interaction/generation/sequence, typed pointer/key/text event | No buffered reconnect replay or shell strings |
| Start/Stop Screen Sharing | Authorized source display/window, immutable receiver IDs | Source/receiver results; explicit ownership and conflicts |
| Apply/Release Attention | Immutable targets, bounded plain message/duration | Per-target apply/release; preserves OS lock |
| Message/Help | Explicit targets/current sender desktop, bounded plain text | Delivery evidence; no HTML or inferred read receipt |
| Open Website/App | Parsed HTTP(S) URL or approved typed catalog entry | Per-target launch acceptance; server does not fetch operator URLs |
| Transfer/File Request | Scoped manifest/chunks/resume identity/destination category | Quota/hash/progress/commit; no arbitrary remote directory browser |
| Screenshot | Authorized current desktop/display, explicit save request | Current image, safe export, metadata audit |
| Wake/OS Lock/Logoff/Restart/Shutdown | Computer IDs, typed operation, deadline/countdown | Per-target effect evidence; no offline destructive queue |
| Recover/Stop Interaction | Exact owned/IT-authorized interaction/computer | Teardown only; no implicit media or general shell |
| Policy Apply | Validated scoped revision | Saved/delivered/applied distinguished |

Unsupported/unknown command values fail typed validation. Support, availability, and permission are independent capability fields.

## Errors

Freeze actual enum values and transport mappings with E03. Required categories: authentication required/expired, permission denied, computer out of scope, interaction ownership conflict/conflict changed, identity mismatch/identity quarantined, desktop changed/session unsupported, capture/input permission missing, protected desktop unsupported, recovery quarantined, release below security floor, protocol/ABI incompatible, invalid input/path/catalog, expired/replayed request, quota reached, offline/timeout, and outcome unknown.

Localize plain human copy with the recovery route. Do not expose secrets, raw SQL, stack traces, or privileged paths. Network errors use appropriate failure status, not a success response with an error hidden in its body.

## Native Rust ownership

The actual C header is an E03 deliverable. Do not guess upstream export names.

1. Use opaque owned handles and fixed-width public fields; no Rust-layout types across ABI.
2. Declare one allocator/release mechanism per output; Swift/C# cannot free Rust memory with their allocator.
3. Async operations have identity/cancellation and explicit completion rules. No callback after disposed ownership.
4. Document threads; native UI dispatches on its main dispatcher. Dispose cannot deadlock waiting on a callback blocked on that dispatcher.
5. Validate frame width/height/format/stride/byte length/display/generation with overflow-safe arithmetic. No raw screens in JSON/logs.
6. GPU handles/fences/cross-process lifetime require measured adapters; CPU proof is not a performance claim.
7. Rust panics/native exceptions do not cross C. Document error conversion, panic mode, and bounded crash teardown.
8. Negotiate ABI compatibility before allocating engine resources. Discard late-generation output while still releasing its buffers.

## Persistence and recovery

Persist identities, access assignments, inventory, flat groups, overview preferences, policy, necessary action metadata, and server files according to D14. Committed transfer jobs and file requests can continue after console closure while their separate authority/deadlines remain valid. An unfinished local upload cannot be called Stored. Revocation/expiry closes the affected file-only authority; it cannot extend media/control authority. Do not persist frames, held input, indefinite reconnect queues, or reusable OS-user passwords.

Migrations require schema/version checks and a consistent pre-migration backup. Restore invalidates live authority through a new deployment generation, reconciles post-backup revocations/enrollments, and cannot silently revive revoked access. A manager restart restores safe preferences, never a serialized Controlling flag. Worker restart validates its actual OS desktop and obtains fresh admission. Server restart reconciles active interaction ownership before resuming admissions.

### File recipient and OS-user boundaries

D29 confirms delivery to the originally intended signed-in person. M13 names the verified local user, enrolled computer, and private destination before dispatch. Persist this binding with the immutable action/manifest and check it again at endpoint commit. A friendly user name, whichever worker connects first, or a changed desktop cannot reinterpret it. This is a confirmed HS queue policy; the [AnyDesk/Veyon comparison](../research-2026-10-09.md#file-delivery-comparison) does not claim either upstream product implements it.

| Version 1 recipient | Destination and delivery | Logout, reconnect, and user change |
| --- | --- | --- |
| Originally signed-in person | Product-owned private destination bound to the verified current console user's stable local identity and computer/enrollment identity, not a display label/session number | Capture the binding at dispatch; no verifiable eligible current user means Recipient Unavailable, not a guessed or historical identity. Logout/user replacement pauses unfinished delivery; only that same revalidated intended user can resume before expiry; no fallback to a shared inbox |

Version 1 exposes only intended-user delivery, with a clear For [User] on [Computer] label. The service owns manifests and temporary content; actual OS ACLs and trusted worker/session selection isolate private committed files and transfer state from other ordinary users. Validate local identity and destination ACL/ownership atomically with commit, and bind any picker destination to that user. A session change between validation and commit fails safely. Never select a personal folder from an operator-provided absolute path. E03/E04/E07 freeze the platform identity/ACL and session-change checks rather than inventing a portable user ID.

A committed server job can wait independently of the sending console, subject to current account/endpoint authority, quotas, retention, and its fixed deadline. Waiting for Recipient is distinct from Delivered; automatic retry cannot extend the deadline or change the recipient. After expiry/cancellation/revocation, the job is terminal and cannot resume. An offline target without a verifiable intended-user binding cannot receive a newly guessed job; the operator sees Recipient Unavailable. V48 must prove the confirmed D29 path before the production delivery slice is Ready.

Waiting for Computer means the bound endpoint is unreachable; Waiting for Recipient means it is reachable but the verified intended user is not on the eligible console. On logout/user change during local transfer, fence the old worker's writes/picker/temporary publication and retain only isolated resumable state. When the same verified user returns, obtain fresh file-only admission for the new eligible desktop generation and revalidate the manifest, deadline, destination ownership, and current authority before resuming. A different account with the same friendly name cannot resume. Already verified Delivered results remain delivered across logout; a lost acknowledgment requires journal reconciliation, not blind repeat. File-only admission never resumes remote control or observation.

Server content access, local delivery, and already exported copies are separate. Removal/retention can delete product-managed originals only under policy; the product cannot promise to erase copies users saved elsewhere. Revocation/identity quarantine/server cutover blocks pending pickup and new delivery. Re-enrolling a replacement physical computer cannot silently inherit pending private delivery; review/cancel or issue a new explicit action for the replacement under current authority, with a freshly verified intended-user binding. A copied key or reused friendly label cannot migrate a queued job.

File-request submissions always remain separate from sent-file delivery. Bind a submission attempt to the authenticated enrolled computer, request revision, and trusted submitting OS-user/session context. This is local attribution, not proof of a named product-person account. User change/logout fences its upload/picker state; a later user cannot resume another person's temporary bytes, replace their committed submission, or download it merely by sharing the computer ID. If no trustworthy person identity exists, label attribution honestly as computer/local-user context and retain separate immutable submission IDs. The server's retrieval permission is for the authorized request owner/reviewer; a submitter sees only their own verified submission state. D21 still owns deadlines, reopening, and version/replacement policy. V48 must test intended-user delivery and submission isolation on actual Mac/Windows user changes.

### Screen-sharing receiver policy

D30 confirms that only the sharing owner or scoped authorized IT can explicitly stop a receiving presentation. The computer user has no product Close/Stop Presentation command. Freeze this policy in each admitted distribution before Starting and enforce it at the endpoint, including native window-close events, menus, keyboard/accessibility actions, IPC, and protocol requests. Hiding a button alone is insufficient. Attention has its own independently authorized restriction; receiving a presentation does not implicitly enable Attention. Later policy changes can stop the branch but cannot silently allow user dismissal or resurrect it.

The native receiver starts as one presentation window fitted to its current display, preserves source aspect ratio, and does not change OS resolution. A reviewed operator/deployment option may request fullscreen for the receiving branch only; neither fullscreen nor a borderless window is proof of enforceable lockdown. Essential source/operator identity and status remain accessible at small sizes and through native keyboard/accessibility paths. Authorized owner/IT stop and recovery controls remain accessible on their own management surfaces without granting them otherwise denied pixels. Cover scaling, multiple displays, fullscreen transition, source loss, and ordinary OS lock/user switching. Every later desktop needs fresh receiving admission; reconnect/user changes never revive a terminal branch.

A receiver desktop ending on lock/logout/user switch is terminal for that branch; presenting to a later desktop requires explicit new operator intent and fresh admission. A transient connection loss on the same still-eligible desktop may attempt bounded fresh receiving admission while the source/distribution authority remains valid; show Unavailable until actual presentation, never replay old buffers or bypass a recorded terminal stop. No admitted receiver means no source capture/encoding demand. If initial recipient admission yields no receivers, report per-target Unavailable/Rejected and end after the frozen preparing deadline rather than sharing indefinitely to nobody.

| Actor or event | Permitted behavior | Branch and operator result |
| --- | --- | --- |
| Computer user | No Close/Stop Presentation control; native close/menu/shortcut/accessibility/IPC/API dismissal attempts cannot end the admitted branch | Presentation remains active; no fake Stopped result; a minimized receiver, where supported, is reported as not currently presenting |
| Sharing owner or scoped authorized IT | Authenticated stop names the exact branch or distribution and checks current stop authority; another ordinary viewer/operator cannot stop it | Branch stop clears only the named receiver after teardown; distribution stop clears all named receivers; operator sees Stopped by Operator/IT |
| Expiry, source/authority/permission loss, desktop end, or relevant process failure | Automatic bounded safety teardown remains effective without user stop rights or available audit storage | Clear content and end the affected branch/distribution with its actual reason; no prevention of ordinary OS protections or claimed tamper-proof enforcement |

Minimizing an allowed receiver window does not stop the branch and is not a completed display acknowledgment: report Presentation Minimized, pause/decrease visible demand through the bounded scheduler, and require current authority/source on restoration. Exact native minimize/fullscreen behavior remains an E04 engineering decision to freeze and prove; D30 selects stop rights, not compulsory fullscreen or blocked local input. Distinguish Receiving on screen, Presentation Minimized, Releasing, Stopped by Operator/IT, Source Lost, and Unavailable. Network packet arrival alone cannot prove presentation.

Authorized branch stop is scoped to that admitted branch/current desktop and does not grant source control, Attention release, or access to another recipient. It is distinct from D09's managed remote-control stop restriction. Concurrent owner/IT stop, policy revocation, denied user dismissal attempts, and late frames must preserve one fenced terminal outcome; cleanup happens even if audit storage is unavailable. A stopped branch remains ended in the original immutable target snapshot; other recipients continue. If the last recipient ends, end distribution/source demand and show Sharing Ended with the reason; do not capture indefinitely for zero receivers. V50 requires the confirmed D30 policy, actual close/minimize/fullscreen behavior, and owner/IT/automatic/race outcomes on both platforms.

### Failed persistence and storage recovery

E02/E06 must freeze required durable commits, disk/metadata reserves, bounded retry/time limits, and actual database/filesystem error mappings. Quota validation alone does not prove that a later write or transaction commit succeeds. Separate file-content quota from the storage needed for security state, journals, and recovery; a large upload cannot consume the entire safety reserve. Check both the database/content volume and temporary/journal volume. Do not depend on available space alone as proof of write health.

| State | Trigger | Allowed behavior and visible result |
| --- | --- | --- |
| Healthy | Required reads, integrity checks, and commits succeed | Normal bounded operations |
| Storage Low | Remaining reserve approaches its frozen threshold, but authority state is still trustworthy | Refuse new content/reservations that would cross the reserve; show Free Storage with scoped safe cleanup; already admitted cleanup/teardown continues |
| Security State Unavailable | Required commit/read fails, database becomes read-only/corrupt, or last trusted revision is uncertain | Latch admission failure; no new grants/renewals, enrollment approvals, credential resets, policy/access changes, file commits, or new side-effect dispatch; existing authority expires and cleanup remains usable |
| Recovering | Scoped IT repairs the actual owned storage/database and checks consistency | Normal admissions stay blocked until recovery proof completes; report actual checks, not Healthy merely because disk space was freed |

Commit action identity, immutable target/payload, deduplication intent, current authority/claim, and any required pre-effect journal before the executing boundary performs a side effect. Freeze which records belong at the hub versus endpoint/helper; a successful hub write cannot substitute for an endpoint's required intent journal. If a required commit fails, refuse the new operation without effects. If an effect already happened and its result commit fails, preserve Outcome Unknown and reconcile the original identity; never automatically repeat it or report Applied from an in-memory optimistic state. File commit verifies durable content and its durable manifest/job before Stored/Submitted; reconcile orphaned temporary objects without publishing them.

Failed access/revocation/reset saves stay Not Saved, with the last committed revision identified. Do not issue new credentials or claim revocation succeeded after a failed transaction. Since that episode makes safe admission/persistence uncertain, stop new grants/renewals and let existing leases terminate; notify administrators of the bounded residual authority rather than promising an uncommitted revocation took immediate effect. Current authenticated stop, held-input release, frame clearing, watchdog release, and emergency typed local IT recovery must not wait for database/audit writes. Keep teardown accessible without creating new observation or general administration authority.

Audit persistence is a separate boundary: when a required security/action audit cannot commit, block the operation that requires it. Diagnostic-log/export failure alone reports Diagnostics Unavailable and cannot block safety teardown. Use only a bounded non-sensitive in-memory warning summary; do not silently grow a fallback log or claim missing events were retained. Replay no sensitive operation after storage recovers. Freeze audit requirements and overflow behavior rather than treating every optional log write as permission to shut down ordinary local computer use.

Operational status separates Process Running from Accepting Authorized Operations. Authority health reports unavailable through the real E03 health interface while the trust store is failed; a successful socket probe does not clear it. Recovery verifies integrity, committed revisions, identity/authority and release checkpoints, pending claim/action reconciliation, file reservation cleanup, and a real harmless write/read transaction in the selected store before reopening admission. Preserve evidence/previous recoverable data; do not auto-reset a corrupt database, delete arbitrary user files, lower security floors, or bootstrap a new administrator to hide failure.

Primary reference checked on 10 October 2026: [SQLite result codes](https://www.sqlite.org/rescode.html) distinguish full disk, read-only, busy, I/O, allocation, and corruption failures, including temporary-file storage. Their precise transaction and recovery behavior must be proved for the selected SQLite configuration. V49 injects failures at commit and acknowledgement boundaries on real storage as well as protocol fixtures; freeing space alone cannot pass it.

### Restore with missing revocation history

An authentic backup proves its contents, not that its accounts/keys/permissions are still trusted. Restore first creates a new recovery/deployment generation through E07, invalidates previous live authority, and keeps restored access in Recovery Quarantine. Restored passwords, MFA/recovery codes, sessions, manager approvals, endpoint identities, access assignments, and file authorities cannot activate themselves. Recovery administration uses local-only or authenticated-tunnel access plus independently held IT recovery authority, not an administrator credential solely recovered from the old backup.

Restoring/moving an authority also follows [active server cutover](#active-server-authority-and-cutover): fresh replacement issuing identity, verified retirement/isolation of the predecessor, and separately recorded peer trust adoption. Generating a new number on the restored server does not fence a concurrently running original or prove that offline peers adopted it.

Normal reconciliation requires authenticated, complete post-backup enrollment/revocation/access-policy history through a trusted current watermark. A partial audit export, missing tail, or backup's own historical watermark is insufficient. Replay current revocations, validate surviving identities and permissions, and retain evidence before releasing each reconciled principal. Deployment-generation changes must also be validated at endpoints; a broadcast alone is not proof of old-authority invalidation.

When complete history is unavailable, permit only fresh IT validation: reset/re-establish account credentials and MFA, freshly approve manager identities, locally verify/re-enroll endpoints with fresh keys, and explicitly rebuild per-computer access. Historical assignments are review hints, never live grants. Unvalidated identities and stored-file access stay quarantined; validating one principal does not release the rest. Lost history is shown plainly and cannot be dismissed into normal operation. If independent recovery authority is also lost, create a new locally trusted deployment and re-enroll; old backup credentials cannot recover trust.

Endpoint recovery provides an explicit local identity-forget action after polling and active endpoint work stop. It removes protected enrollment metadata and the device signing key while retaining files already received. The old computer record and queued work remain attached to the old server identity; the Manager must quarantine it and issue a fresh invitation for a new endpoint identity. Local deletion alone does not revoke the server record or prove old-host fencing.

Keep the prior installation recoverable and restored content protected until its access policy is reviewed. Recovery completion requires actual denial of old/revoked identities, fresh admission for the validated subset, and retention of current release-security floors. No media/control/sharing/attention resumes from the backup.

### Signed updates and authorized rollback

E08 must freeze a minimumSecurityRevision per compatible component/platform and the release authority that advances it. This is a monotonic security counter, separate from display version and ABI/schema compatibility. Signed release metadata binds artifact hash/size, component/platform, source pins, release sequence, security revision, compatibility, and trusted metadata versions. Use a maintained reviewed verifier; these semantic fields do not prescribe an invented signed format. The [TUF specification and client workflow](https://theupdateframework.github.io/specification/latest/), checked on 10 October 2026 at specification 1.0.37, is a primary reference for metadata rollback/freshness protection, not a completed library-adoption decision.

The server verifier adopts TUF targets metadata for this binding. A release manifest target is named `releases/<platform>/<version>/manifest.json`; its `custom.helicoid_release` object carries `format_version`, `product`, `version`, `platform`, `source_commit`, `release_sequence`, `security_revision`, `minimum_security_revision`, `min_schema_version`, `max_schema_version`, and `min_installer_version`. The sibling `manifest.sig` and every declared `artifacts/*` file must also be TUF targets. The root metadata is supplied out of band, TUF expiration checks stay enabled, and the TUF datastore is persistent under the installer state directory. `hs-server installer verify-tuf-release` verifies and stages these targets, then verifies the existing Ed25519 package signature. It does not install the package or claim that update, rollback, or local security-floor enforcement is complete; lifecycle capabilities remain false until those apply-time checks and health gates are connected.

Ordinary install, repair, update, offline import, and restored-host startup must reject an artifact below the effective security floor even when correctly signed. Compute that floor from the highest applicable trusted release policy and surviving server/local records. Never lower it through configuration import, old database/backup restore, reinstall, or an older manager. Retain protected local checkpoints and separately stored recovery evidence; after total loss of trusted floor state, require a freshly authenticated release-policy bundle through independent IT recovery before admitting normal operation. Do not assume an old backup contains the current floor. A local administrator capable of replacing binaries/storage is outside this software-only anti-rollback guarantee.

Ordinary release metadata cannot roll back to a lower trusted metadata version. Ordinary update/reinstall cannot reduce highestAcceptedReleaseSequence; repair of the identical currently installed approved artifact is allowed only while it meets the security floor. Authorized rollback is a separate scoped IT operation with fresh MFA, reason, exact artifact hash/version, fixed targets, current policy, and bounded validity. It drains interactions and proves code/data compatibility before installation. It may select an earlier functional version only through fresh trusted release metadata, and only when that artifact's security revision meets the current floor. It preserves the release-sequence high-water mark and cannot lower the floor, accept a vulnerable release, bypass signatures, or make stale metadata current. Publish a patched/backported compatible artifact if the old functional release is below the floor. Interrupted rollback retains the floor and reports its actual terminal/unknown result without retrying automatically.

The product-controlled installer/updater and privileged apply boundary must enforce these checks before invoking an artifact's installation code. Running an arbitrary historical installer with local administrator authority is not an enforcement path the product can secure by itself.

E07/E08 must establish signed offline metadata freshness, trustworthy time/recovery handling, storage integrity, and platform enforcement. Moving the wall clock or losing floor records is a recovery failure, not permission to weaken validation. Freeze numeric revisions and real compatible rollback artifacts before S09; no invented revision number or claimed guarantee substitutes for V45.
