# Build dependency map

Draft updated on 10 October 2026. This is a dependency map; implementation evidence is recorded separately and a listed slice is not automatically complete. main coordinates separate owning repositories and tested pins. No repository, package name, ABI export, toolchain command, or signing identity is invented by this document.

> **Manager migration overlay, 21 October 2026:** [ADR 0008](../adr/0008-shared-qt-manager.md)
> and the user's Helicoid core v1 plan supersede the native Manager framework
> and presentation assumptions below. The existing slices retain their core,
> endpoint, authority, and release proof requirements. The Qt Manager has an
> independent CMake source target at `apps/manager`, with host-gated macOS
> arm64/Intel and Windows x64 build presets pinned to Qt 6.10.x; its C++ build,
> packaging, and UI runtime remain unverified until the matching SDKs are
> available.

## Proof prerequisites

| ID | Owning area | Inputs | Required output before dependent implementation |
| --- | --- | --- | --- |
| B01 | main and maintainers | Confirmed decisions | Repository/package identities, support inventory, license direction, role-specific icon source assignments including D28, real independent build/CI commands |
| B02 | core, native apps | B01; E01/E04; confirmed D26/D27 | Pinned engine/native bridge proof, actual header/lifetime rules, signed console-session capture/input and focused-canvas/local-input matrix, adoption/fallback decision |
| B03 | core/server/native apps | B01; E02/E03/E07 | Generated bounded protocol/ABI, auth/bootstrap/grants/replay, administrative delegation/reset ceilings, per-computer scopes, complete conflict arbitration, incarnation quarantine/replacement, active-authority transition/checkpoint, required durable commit/failure modes, error mappings |
| B04 | server/native setup | B01; E05/E07/E08 | Supported host/package matrix/resource manifest/W17 routes, Ready versus Verified, exact first-manager bootstrap, missing-history quarantine and independently trusted cutover/old-host fencing, security-floor/checkpoint and authorized rollback contracts, role-specific Mac icon integration/fallback, safe install/update/remove |
| B05 | core/server/apps | B02/B03; E06 | Reference benchmark, frozen limits/timeouts/queues/leases and storage reserves, overload/freshness and persistent-failure recovery behavior |

Design artifacts may proceed before these pass. Production callers must not be written against imagined exports or unresolved security formats. B02 and B03 both gate remote control; an unrestricted generic RustDesk launch is not a vertical slice.

The [RustDesk integration plan](remote-desktop.md) expands E01/E03/E04/E06/E07 into RD01–RD12 with observable proof requirements. RD01/RD02 establish source/build/ABI feasibility, RD03 establishes the authenticated route and endpoint authority, and RD04 proves real native observation. Subsequent packages cover control, mixed media, platform/load, hosting, and maintenance before an adoption decision. RD packages are local planning work, not completed prerequisites or published tickets.

## Vertical slices

| ID | Slice and owning repos | Dependencies | Observable completion |
| --- | --- | --- | --- |
| S01 | Native skeleton, local account/trust, endpoint and manager enrollment; apps/server/core | B01/B03/B04, D20/D28 | Guided server reaches Ready for Enrollment without second machine on every W17 route; verified identity/bootstrap, clone quarantine/replacement, scoped account/manager authority and no denied pixels; V02/V03/V23/V25/V43; V39 readiness, V46/V49 core persistence, and V51 Mac manager icon checkpoints only |
| S02 | Mac manager automatically previews one Windows endpoint; apps/core/server | S01/B02/B05, D26 | Actual admitted physical-console preview on console opening, stale/expiry/desktop-change and multi-session exclusion recovery; V01/V04/V07/V11/V12/V40; V39 authorized media and denial checkpoint, still incomplete |
| S03 | View-only and explicit unique control; apps/core/server | S02, D19/D24/D27 | Real Take/Stop Control, minimize releases control/keeps viewing, restore requires explicit control, focused-canvas routing with local input enabled, no managed-user stop, IT stop-only, input/dispose/crash proof; V08–V12/V24/V29/V37/V42; V39 completes at Deployment Verified after real control teardown and recovery |
| S04 | Both native managers/endpoints, Veyon-style inclusion, search/Grid/List/simple groups; apps/core/server | S02/S03/B05, D18/D25/D26/D27 and detailed native selection contract | Full architecture and session/input matrix, primary-display previews, separate inclusion/selection, filtered default targets, fixed action scope, 30/60 preview report; V05/V06/V30/V31/V34/V35/V38; V40/V42 complete after all supported combinations pass |
| S05 | Sharing, attention, message/help, website; apps/core/server | S03/S04, confirmed D30, frozen conflicts/enforcement | Actual fan-out/receiver, endpoint-enforced owner/IT stop, proved minimize/fullscreen behavior, final-recipient source teardown, bounded release, symmetric transition matrix, plain delivery/help and user-context launch; V13–V17/V50; V41 desktop-interaction and V49 dispatch checkpoints |
| S06 | Server files, requests, received files; apps/core/server | S01/S04, confirmed D14/D21/D29 | Quotas/safe atomic content/scoped retrieval/close/replacement/backup, immutable intended-user recipients, bounded wait/resume for that user, cross-user submission isolation, committed jobs independent of console; V18–V20/V32/V36/V48; V49 file-commit checkpoint |
| S07 | Approved apps, screenshots, machine actions; apps/core/server | S03/S04/S05, typed helper/catalog and complete conflict matrix | Real supported OS effects, machine-action conflict reservations/teardown and unknown-result reconciliation; V17/V21/V22/V24; V41 completes after power and all matrix cells pass |
| S08 | Fleet policy/accounts/import/export/support; server/apps | S01/S04 | Scope/revision/delegation/reset/import enforcement and redaction, explicit self/security review, optional groups independent from access; V05/V23/V32; V46 complete and V49 administration checkpoint |
| S09 | Production install/update/backup/restore/remove; server/apps/core | B04, S05–S08 | Occupied-host, missing-history quarantine, actual old/restored-server cutover/fencing/offline reconciliation, signature-valid downgrade refusal, persistent security floors, safe rollback and recovery matrix; V26–V28/V33/V44/V45/V47; V49 complete and V51 installed update/fallback matrix |
| S10 | Full version 1 release; all | S01–S09 | All promised core workflows, native accessibility, reference budgets, pilot and independent security evidence |

S02 is a proof milestone, not a finished product. S10 includes the broader tools the user retained. Advanced recording/audio/restrictions/external identity/pre-login integration receive their own contracts and gates after version 1.

V39 stays incomplete through S01 and S02. S03 may mark it Passed only when retained evidence covers setup readiness, enrolled endpoint authorization, authorized media, denial, control teardown, and recovery for the supported W17 hosting routes. Earlier checkpoints cannot substitute for the final evidence or mark H13 Deployment Verified.

V40–V45 are mandatory boundaries, not optional hardening after release. S02/S03 retain prioritized Mac-manager/Windows-endpoint checkpoints for V40/V42; S04 completes their full supported platform/architecture matrix. Earlier checkpoints cannot mark those full scenarios Passed. V41 stays incomplete at the S05 checkpoint until S07 proves machine actions and every coexist/refuse/replace cell, including concurrent foreign-owner claims. S09 must prove V44/V45 with real restore/update artifacts before production installation/recovery claims. Protocol fixtures cannot replace OS effects or installer evidence.

V46–V51 extend those gates. S01 records bounded bootstrap/admin/storage and Mac manager icon checkpoints; it cannot pass full later-feature/architecture scenarios. S04 contributes V51's complete Mac manager build/display matrix; S09 completes its installed update/fallback evidence. S08 completes V46, S06 proves confirmed D29 through V48, S05 proves confirmed D30 through V50, and S09 completes V47/V49 using retained earlier write/effect tests. Confirmed product choices are not passing runtime evidence, and a copied icon source is not compiled native evidence.

## Per-slice implementation handoff

Every future ticket names: exact owning repository; user trigger/result; relevant W/M/E/A/H/P/V IDs; settled product choices; frozen generated contract version; preconditions; allowed state transitions; error/cancel/reconnect rules; persistent data/migration; resource/security bounds; real acceptance commands; retained evidence; and known unsupported platform behavior.

Implement the smallest end-to-end slice that proves its boundary, then update main's pins only after integration evidence. The next agent reads current decisions and does not revive superseded rooms/classes/lessons from an older mockup or research paragraph. Ready means this slice's product and proof prerequisites are settled, not that a large document exists.
