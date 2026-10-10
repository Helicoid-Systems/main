#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
scope="${1:-all}"

verify_rust() (
    cd "$workspace_root/modules/$1"
    cargo fmt --check
    cargo test --locked
    cargo clippy --locked --all-targets -- -D warnings
)

verify_macos() (
    cd "$workspace_root/apps/macos"
    swift test
    swift build --product HelicoidManager
    swift build --product HelicoidComputer
)

verify_windows_domain() (
    cd "$workspace_root/apps/windows"
    dotnet test src/Helicoid.Domain.Tests/Helicoid.Domain.Tests.csproj -p:RestoreLockedMode=true
)

case "$scope" in
    core|server) verify_rust "$scope" ;;
    macos) verify_macos ;;
    windows-domain) verify_windows_domain ;;
    all)
        verify_rust core
        verify_rust server
        verify_macos
        verify_windows_domain
        ;;
    *)
        printf '%s\n' 'Usage: scripts/verify.sh [all|core|server|macos|windows-domain]' >&2
        exit 2
        ;;
esac

# Native Windows builds and real endpoint/media tests have separate platform gates.
