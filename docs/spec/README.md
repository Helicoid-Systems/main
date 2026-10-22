# Implementation specification

Working specification for the native computer monitoring product. Updated on 19 October 2026. Automatic authorized previews, optional flat groups, Veyon's computer-management workflow, multiple viewers/one controller, one-time manager approval, server file storage, physical-console-only sessions, and enabled local input during control are confirmed. There is no room/class/lesson model. The accepted UI00 full74-family native redesign expands every screen family in the screen specification and records acceptance status separately from source. Remaining product choices and engine/platform proof gates are explicit; this is not a built product or fully frozen specification.

## Read in this order

| Document | Purpose |
| --- | --- |
| [Decisions](decisions.md) | Confirmed answers, unresolved product choices, and evidence-dependent engineering choices |
| [System contracts](contracts.md) | Invariants, data ownership, authority, state transitions, messages, and resource ownership |
| [Workflow contracts](workflows.md) | Exact steps, validation, persistence, cancellation, and errors for the user-visible workflows |
| [Verification](verification.md) | Observable acceptance scenarios and the evidence required for a claim to pass |
| [Build map](build-map.md) | Dependency order, deliverables, and the evidence needed before each implementation slice starts |
| [Remote desktop integration plan](remote-desktop.md) | Historical RustDesk specialization into RD01–RD12; the accepted engine route is native Google WebRTC per ADR 0007 |
| [Screen specification](../ui.md) | The 74 screen families with layout, actions, unavailable and failure behavior, and focus |
| [UI acceptance ledger](../evidence/ui-acceptance-2026-10-19.json) | Per-screen source/build/runtime status, dependencies, phases 0–8, and production gates; initially Not Verified |

The [product plan](../plan.md) supplies context and rationale. [CONTEXT.md](../../CONTEXT.md) defines terms. The research ledgers record upstream evidence and its limits.

## Rules for an implementation agent

1. Identify the slice being implemented and read its prerequisites, contracts, screen IDs, and acceptance scenarios. Do not translate the whole plan into a large unreviewable patch.
2. Follow confirmed user decisions. A newer explicit user correction takes precedence; update affected decisions and contracts together.
3. Do not present a recommendation or a mocked interaction as proven platform behavior. An unresolved product choice stays unresolved. A platform unknown becomes a proof task with recorded results.
4. Do not invent an engine export, wire field, signed-grant format, service identity, installer capability, or command that the chosen dependency does not expose. Freeze generated schemas and real headers through the designated prerequisite task.
5. Enforce authority in the hub, endpoint, and privileged helper where required. Disabling a button alone does not enforce a permission.
6. Keep native UI state separate from remote state. Issued, delivered, applied, rejected, timed out, and outcome unknown are different results.
7. Preserve existing repository changes. Build independently in the owning implementation repository; update main's exact pins only when integration evidence supports them.
8. Add tests at the interface that can catch the named failure. Real capture, permissions, input, and installation claims need real platform evidence. A fixture filled with screenshots cannot satisfy them.
9. Record the command, environment, version/commit, result, and remaining limitations. Never mark a gate passed merely because a build succeeded or a checklist was copied. Never mark a capability Verified from documentation alone.
10. Deliver the requested slice and stop at its stated scope. Optional or future features stay hidden from normal version 1 navigation until their phase ships.

## Status vocabulary

| Status | Meaning |
| --- | --- |
| Confirmed | The user explicitly selected the product behavior |
| Draft contract | A concrete engineering design for review; not an upstream/runtime fact |
| Needs answer | A product choice still awaiting the user's answer |
| Needs proof | Real dependency/platform behavior must be measured before the design can be frozen |
| Ready | Product prerequisites and engineering contracts for this particular slice are settled |
| Verified | The named acceptance scenario passed with retained evidence |
| Not Verified | No retained source, build, or runtime evidence yet; documentation specification does not count |
| In Progress | Work is underway; separate from any validation status |

Readiness is per slice. The presence of detailed prose never automatically makes the entire product Ready.

V40–V45 make OS-session selection, conflict arbitration, local/remote input, copied identity competition, missing-history restore, and signed-release anti-rollback mandatory proof gates. Read their canonical sections in System contracts before implementing the corresponding slice; a default engine behavior cannot override them.

V46–V50 add scoped administrative delegation/recovery, single-active server cutover, cross-user file recipients/submissions, required persistence failure, and sharing receiver policy. D29 confirms queued sent files waiting for their originally intended user; D30 confirms presentation stop only by the sharing owner or scoped authorized IT. Their dependent slices still need actual V48/V50 proof before Ready. D28 confirms the new role-specific Mac manager icon, with V51 covering actual native compilation/display/fallback. All 51 verification scenarios remain evidence requirements, not results already passed.
