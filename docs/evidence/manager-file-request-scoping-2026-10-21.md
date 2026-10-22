# Manager file-request identity scoping — 2026-10-21

## Change

Combined file-request rows now use `ScopedFileRequestId`, pairing each
server-local request ID with the authenticated deployment identity. Table
selection carries both the owning profile and deployment identity, then
resolves the raw server-local ID only for that server's request path. Refresh
and session cleanup remove request rows by deployment identity.
Publish and close actions snapshot the selected row before modal dialogs, so a
refresh during the nested event loop cannot invalidate a row iterator used by
the confirmed mutation. The server's expected revision still detects a
concurrent change.

## Verification

- Source inspection confirms list refresh, sorting, selection, mutation, and
  deployment cleanup use the typed scoped identity.
- A direct trailing-whitespace scan reported no matches in the touched files.
- The registry compiled with `-std=c++20 -Wall -Wextra -Werror -pedantic`, and
  an isolated `ScopedFileRequestId::create` use compiled.
- `main_window.cpp` is not compiled or runtime-verified because Qt 6.10 is
  unavailable in this environment.
