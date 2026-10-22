---
status: accepted
date: 2026-10-20
---

# Shared Qt management console

The user's Helicoid core v1 plan selects one shared Manager application for
Windows and macOS, built with C++20, CMake, Qt Widgets, and SARibbon's Office
2021 theme. Qt 6.10 is the initial compatibility line. The implementation
source currently lives in `apps/manager` and configures independently; creating
or attaching a separate remote repository remains an integration step.

This replaces the management-interface portions of ADR 0001 and the previous
native Manager UI plan. It does not replace the platform-specific Computer
status applications, capture/input workers, privileged helpers, or shared Rust
engine. The existing macOS and Windows repositories keep those responsibilities
until the new Manager reaches accepted feature parity, after which their
Manager targets can be retired.

The Qt application owns presentation, client coordination, and viewer windows.
Platform adapters own secure key storage, native integration, hosting, and
capture/input. The Rust engine remains behind its generated native interface;
callers must use only exports present in a pinned, built interface.

Manager-local profile IDs are distinct from the server-owned deployment ID.
Combined records use scoped keys containing the authenticated deployment ID
and unchanged server-local entity ID. Async session work carries a generation
for that deployment. A health response only establishes reachability; it does
not create an authenticated session, authorize a computer, or admit media.

The current source at `apps/manager` provides the main console shell, shared
workspaces, File menu, local connection profiles and setup draft, explicit
profile import/export, and bounded availability checks. Sign-in uses the
server-owned deployment ID, v2 challenge/completion, the Manager device's
protected signing identity, and server-requested MFA. Each deployment has an
independent in-memory session; combined account, manager-device,
access-assignment, computer, file-job, and file-request records are scoped to
the authenticated deployment and fenced by session and refresh generations.
Computer inventory also has Grid/List, search, server, and enrollment-state
filters. Account, manager-device, and access-assignment mutations are wired to
their owner-scoped v2 routes and require the server's authorization; sensitive
administrative writes also require recent MFA. The selected server has a detail
pane; forgetting a profile removes only local state.

The setup workspace saves a local draft and offers a read-only SSH host
preflight using existing strict `known_hosts` trust. It does not apply over
SSH; the host check does not inspect the selected data mount, network interface
assignment, or TLS files.
Compose installation, backups, updates, viewer streams, endpoint admission, and
control remain unavailable. The Qt source has not been built in the current
workspace because Qt 6.10 is not installed there; source presence does not
establish desktop behavior or acceptance.

Profile migration exports the bounded `helicoid.manager.migration` schema-v2
bundle and still accepts the earlier schema-v1 profile-only JSON. Version 2
carries profiles, scoped monitoring inclusion preferences, and the global
computer Grid/List preference. Import also accepts earlier schema-v2 bundles
without the optional view preference. It omits trust
pins, credentials, live sessions, device signing keys, and the machine-local
Linux setup draft. Import requires review and confirmation, re-verifies server
profiles newly added on the destination, retains pins for matching local
profiles, and replaces matching preference scopes. The
macOS Qt adapter shares the previous Swift Manager's Keychain item and upgrades
the earlier Qt adapter's raw seed format in place, so the same Mac reuses its
protected Manager identity without file export. The Windows Qt adapter uses
Credential Manager; the inspected predecessor has no matching persisted
signing-key adapter, so a new identity may need normal server approval. Neither
path exports a private key. This is source-level migration support until the
Qt artifact is built and the cross-platform import/export workflow is
exercised.

SARibbon is pinned to source revision
[`806e3e93be4dd7676697d3017282a4359519e053`](https://github.com/czyt1988/SARibbon/releases/tag/v2.9.0).
Qt's published Qt 6.10 support table lists Windows 10 (1809+) and macOS 13+
among its desktop targets: [Qt 6.10 supported platforms](https://doc.qt.io/qt-6.10/supported-platforms.html).
