# Server TUI LAN endpoint diagnostics — 2026-10-22

## Implementation

Opening or refreshing the Network and certificates screen still reads bounded
local interface and resolver information. For a saved LAN configuration, it
now resolves the configured Manager hostname (or private bind IP) and attempts
a TLS 1.2+ handshake to the configured authority port. The handshake uses the
current process's system trust store and checks the configured hostname. It sends no
credentials and reports only bounded private resolved addresses, connection
status, TLS version, and the verified leaf certificate's SHA-256 fingerprint.
DNS and connect work share a five-second deadline; at most eight private
addresses are attempted.

Invalid or non-LAN settings skip the endpoint probe. The screen does not edit
host configuration or inspect firewall rules and public routing.

## Verification

- Go tests cover valid/invalid target selection, successful TLS verification
  and leaf fingerprinting, hostname-mismatch rejection/configuration, direct
  IP handling without DNS, private-address filtering, and bounded address
  de-duplication.
- `GOCACHE=/private/tmp/hs-helicoid-tui-gocache-20261022 go test ./...`
  passed.
- `GOCACHE=/private/tmp/hs-helicoid-tui-gocache-20261022 go vet ./...`
  passed.

The TLS fixture uses `net.Pipe`; no live server, private DNS, LAN firewall, or
certificate deployment was exercised. The runtime screen behavior remains
unverified on an installed host.

## Compose observation boundary

The TUI now labels the diagnostic vantage as its own process network namespace.
When run inside the Compose container, the interface/resolver listing and TLS
probe describe the container and use its trust store; they do not establish the
host's interfaces, resolver, trust bundle, firewall, or reachability.
