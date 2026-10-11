# Helicoid development workspace

Helicoid uses native macOS and Windows applications with separately pinned core and server repositories. See [BUILDING.md](BUILDING.md) for development commands and [current evidence](docs/implementation-2026-10-10.md) for exact source revisions, checks and limits.

The current work includes a persistent issuer/receiver admission candidate, an isolated RustDesk frame/callback lifetime bridge, Windows metadata-worker fencing and Mac hosting preflight. These are prerequisites. Live preview and control, public TLS/rendezvous/relay, installed hosting, updates and recovery remain unfinished. The selected stock RustDesk relay fails the required adoption gate; no alternate transport or public listener has been enabled.

The implementation scope includes all four Mac/Windows manager–endpoint combinations and Linux SSH, Windows-local and macOS-local hosting. [The scope decision](docs/adr/0006-local-macos-hosting-and-public-routing.md) records these additions; it does not establish platform proof. Server Ready for Enrollment and Deployment Verified are separate states.

Download the [Apple Silicon development apps](https://github.com/Helicoid-Systems/macos/actions/runs/38167550626/artifacts/11711859570) or [Windows x64 development apps](https://github.com/Helicoid-Systems/windows/actions/runs/38142367280/artifacts/11703970176). These are prerequisite builds, not completed worker/admission, live-preview or control checkpoints. Windows CI ran on Windows Server 2025; Windows 11 installed testing remains outstanding. Mac builds use ad-hoc signing, and Intel compilation does not establish Intel runtime acceptance.

Production release waits for platform signing/notarization, installed acceptance, recovery and security review. The supplied Ubuntu VM has received no Helicoid deployment mutations; its existing services and data remain intact.
