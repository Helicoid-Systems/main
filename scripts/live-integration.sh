#!/usr/bin/env bash
set -euo pipefail

workspace_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ $# != 1 ]]; then
    printf '%s\n' 'Usage: scripts/live-integration.sh /absolute/path/to/hs-server' >&2
    exit 2
fi
server_binary="$1"
dotnet build "$workspace_root/scripts/client-smoke/windows/ClientProof.csproj" --nologo
client_probes=(--client-probe dotnet "$workspace_root/scripts/client-smoke/windows/bin/Debug/net10.0/ClientProof.dll")
if [[ "$(uname -s)" == Darwin ]]; then
    swift_probe="$workspace_root/scripts/client-smoke/mac-client-proof"
    xcrun swift build --package-path "$swift_probe"
    client_probes+=(--client-probe "$swift_probe/.build/debug/ClientProof")
fi
python3 "$workspace_root/scripts/server-smoke.py" "$server_binary" "${client_probes[@]}"
