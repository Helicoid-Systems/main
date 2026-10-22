# Manager computer inclusion source checkpoint — 2026-10-21

## Implemented

The shared Qt Manager now has a collapsible Computers panel with an All
Computers control and deployment-scoped group and individual checkboxes.
Group state is derived from the included member IDs, so overlapping groups stay
consistent when the same computer is changed from either group. Computers
without memberships appear under Ungrouped Computers. The panel and inclusion
mode are persisted per authenticated deployment and account in local
application settings.

All mode includes newly authorized inventory after refresh. Explicit mode
contains only selected computer IDs; IDs absent from the current authorized
inventory are pruned, and newly appearing computers remain excluded. Inclusion
is applied before the existing server, search, enrollment-state, and group
filters. It remains local display state: it does not grant access or start
capture. The panel states this directly while the native media path is
unavailable. Its initial visibility follows the user's per-account setting;
when no setting exists, it opens automatically only when authorized inventory
has groups.

## Verification and boundary

- Source implementation is present in `apps/manager/src/main_window.cpp` and
  `apps/manager/src/main_window.h`; the Manager README describes its behavior.
- The Qt Manager was not compiled in this environment. The current build
  evidence records that Qt 6.10 is unavailable.
- No native UI, accessibility, or persistence runtime acceptance is claimed.
- This does not deliver endpoint admission, preview pixels, or capture
  teardown; those remain separate protocol and media-path gates.
