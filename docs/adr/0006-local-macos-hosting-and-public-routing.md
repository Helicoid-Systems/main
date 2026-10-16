# Local macOS hosting and public routing

Status: Accepted product scope, 10 October 2026. Implementation proof remains required.

Update, 13 October 2026: [ADR 0007](0007-native-webrtc-and-public-edge.md) supersedes the RustDesk route below with native Google WebRTC and a separate two-address public edge. The hosting scope and occupied-host preservation requirements remain authoritative.

The authorized preview/control implementation includes all four Mac/Windows manager–endpoint combinations, Linux hosting over SSH, Windows-local hosting and macOS-local hosting. This supersedes the earlier exclusion of a local macOS coordination server and deferral of public remote access.

The Mac setup assistant offers This Mac and Existing Linux Server. Local hosting uses a product-owned launchd service; it does not move capture or input into a system daemon. Windows hosting remains local to the Windows app. All three installers share bounded review, resource ownership, journal, readiness, update and recovery semantics.

Public connections use organization-owned OSS rendezvous and relay services with independently pinned deployment trust and endpoint admission. The selected RustDesk secure route must prove exact peer/channel binding and reject legacy or insecure continuations. Failure stops adoption rather than automatically selecting a different transport. Relay allocation requires bounded authenticated authority; an open relay is not a deployment milestone.

The supplied Ubuntu 26.04 x64 VM is an occupied host. Existing services, data and listeners, including ports 80 and 443, must survive the Helicoid installation. Default authority port 8920 and required routing ports are reviewed and rechecked before apply. Bootstrap is local-only or carried through verified SSH, not an unauthenticated public setup page.

Development builds and isolated trials can proceed without platform signing credentials. A production release still requires signing/notarization, installed platform acceptance, recovery and security evidence. Intel source compilation does not replace installed Intel runtime checks. Sharing, attention, files and the other broader version 1 tools retain their existing independent delivery gates.
