# Computer monitoring workspace

Current verification is recorded in [implementation evidence](docs/implementation-2026-10-10.md): these are development prerequisites, with live media/control and production deployment still gated. [The accepted scope addition](docs/adr/0006-local-macos-hosting-and-public-routing.md) includes local Mac hosting and organization-owned public routing. The selected engine and public-edge route is native Google WebRTC, recorded in [ADR 0007](docs/adr/0007-native-webrtc-and-public-edge.md); earlier RustDesk planning and laboratory evidence are historical.

This repository is the planning and integration workspace for Helicoid, an open-source computer monitoring and remote assistance product for macOS and Windows from Helicoid Systems.

The shared Manager is moving to C++20, Qt Widgets, and SARibbon's Office 2021 theme in [apps/manager](apps/manager/README.md), per [ADR 0008](docs/adr/0008-shared-qt-manager.md). The macOS and Windows repositories retain their platform-specific Computer applications and workers; their Manager targets retire only after accepted feature parity. The shared Rust engine remains behind its generated native interface.

The main workspace is Computers. Access applies directly to preconfigured computers, without rooms, classes, or lessons. The current deployment scope includes local networks and public internet connections through organization-owned rendezvous and relay, with administrator-managed endpoints. The earlier native Manager screen specification remains a behavior reference while its framework, navigation, and ribbon presentation are replaced.

Computer management follows Veyon's inclusion, preview grid, and selected-or-displayed action workflow, with optional flat groups and native presentation. The [source review](docs/research-2026-10-09.md#veyon-source-review-computer-management) pins the inspected upstream revision; [ui.md](docs/ui.md) defines the adaptation for all 74 screen families.

- [Product and engineering plan](docs/plan.md): behavior, native interfaces, remote desktop, configuration, self-hosting, security, quality, and delivery gates.
- [Implementation specification](docs/spec/README.md): decisions, invariants, workflows, verification requirements, and build dependencies.
- [Remote desktop integration plan](docs/spec/remote-desktop.md): historical RustDesk specialization; the accepted route is native Google WebRTC per ADR 0007.
- [Screen and interaction specification](docs/ui.md): behavior, unavailable and failure contracts, and focus; manager presentation is being migrated to the shared Qt shell.
- [UI acceptance ledger](docs/evidence/ui-acceptance-2026-10-19.json): per-screen source, build, and runtime status with phases 0–8; initially Not Verified.
- [Native UI checkpoint](docs/evidence/native-ui-2026-10-20.md): implemented composition/inventory work, artifact hashes, reproduced checks and remaining runtime gaps.
- [Domain glossary](CONTEXT.md): the vocabulary used throughout the plan.
- [Research and evidence](docs/research-2026-10-09.md): primary sources, conclusions, and unresolved feasibility questions.
- [Architecture decisions](docs/adr/): accepted constraints and proposed decisions.
- [HS branding assets](assets/brand/manifest.json): original icons/logos and checksums, including the newly selected Mac manager icon and explicit platform/role assignments. Placement and native integration are defined in the screen specification.

Core, server, macOS Computer, and Windows Computer code remains in four independently buildable repositories, checked out here as pinned Git submodules. The Qt Manager source configures independently from `apps/manager`; it is not yet attached as a separate remote submodule. See [BUILDING.md](BUILDING.md) for build entry points and [implementation evidence](docs/implementation-2026-10-10.md) for the verified foundation and its limits.

The current implementation wave targets admitted native preview/control, public-network routing and Linux, Windows and local macOS hosting. These capabilities remain unavailable until their authority, transport and installed-platform gates pass. Signed production release and Intel runtime support require additional signing credentials and device evidence. The broader version 1 tools and performance budgets retain their separate proof requirements.
