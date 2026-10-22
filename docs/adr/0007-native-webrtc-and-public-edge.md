# Native WebRTC and the public edge

Status: Accepted implementation direction, 13 October 2026. Runtime adoption is not yet verified.

The finishing plan selects native Google WebRTC behind the shared Rust/C interface, with native Swift and WinUI applications. This supersedes the RustDesk route selected in ADR 0006. Retain the RustDesk laboratory and its failed adoption evidence as historical: its inert ABI/lifetime checks do not verify WebRTC or normal observation/control.

The engine uses independently pinned upstream source, dependencies and toolchain revisions. ABI 3 exposes only implemented exports, with generated headers and matching native wrappers. Verified-channel authority originates from the engine's connected, certificate-verified DTLS transport, never a caller-provided SDP fingerprint. Admission must be consumed at the endpoint before capture or input becomes available. CPU frame handoff and software VP8 are the initial route; unneeded upstream capabilities remain disabled.

The bounded product data channel is usable only while the engine holds a current verified-channel binding. Drop inbound bytes received before pinning, clear queued bytes when that binding is invalidated or the channel closes, and serialize sends with pin replacement and release. This transport identity check does not replace server-issued endpoint admission for capture or input.

Public access uses a separate organization-owned Ubuntu edge VM with two static public IPv4 addresses. IP 1 serves HTTPS/WSS authority carriage and signaling on TCP 443; IP 2 serves TURN over TLS on TCP 443 with a bounded UDP relay-port range. The supplied Ubuntu authority host and local Windows/macOS authorities retain account/device authorization, durable state and signing keys. Authority port 8920 remains the default. Existing services, data and ports 80/443 on the supplied VM remain intact.

Each authority opens an authenticated outbound connector to the edge. Clients establish inner TLS through a bounded authority-only carrier; TLS terminates at the actual authority and validates independently enrolled deployment trust and certificate identity. The edge does not receive plaintext authority passwords or session tokens. Routing cannot select an arbitrary destination, and public routing cannot bootstrap deployment trust.

TURN credentials require authenticated issuance, allocation and bandwidth budgets, destination restrictions and a renewable authorization lease capped at 60 seconds. Existing allocations must terminate within the authorization bound when renewal stops, including retained credentials and Refresh attempts. Credential expiry alone does not prove allocation expiry. If stock coturn fails this contract, a narrowly scoped maintained lifecycle integration is required before adoption. Endpoint admission expiry and safety cleanup remain independent of relay cleanup.

Administrator MFA remains mandatory. Operator MFA is configurable and optional by default, including public deployments. Approved device-key proof remains mandatory for production challenge login. Optional policy cannot silently strip protection from an already-active enrolled factor. Recovery credentials remain recovery-only.

The release gates remain signing/notarization, installed Windows/Apple Silicon acceptance, Intel runtime acceptance, actual two-host recovery/cutover and independent security review. Development prerequisites and cross-compilation do not satisfy these gates. All four manager/endpoint combinations and Linux SSH, Windows-local and macOS-local hosting remain in scope; sharing, attention, files and the other broader version 1 tools remain separate.

Public deployment additionally requires an owned edge VM, two addresses, DNS/certificate-renewal access and a bandwidth budget. These inputs are not presently configured by this decision. No public capability becomes available merely because the direction is accepted.

## Available infrastructure, 16 October 2026

The user has only the supplied `ubuntu@57.131.161.27` VM. Fresh read-only SSH
preflight found one public IPv4 address, a global IPv6 address and existing
`traefik:v3.6.14` (`runtipi-reverse-proxy`) owning ports80/443 on both families.
Authority port8920 is free. No host configuration, service, data or firewall
was changed; the separate two-address edge is unavailable.

A single-VM alternative is **proposed, Not Verified**, rather than an accepted
replacement architecture. Dedicated authority/carrier and TURN DNS names could
use exact-name TCP TLS SNI passthrough in the existing proxy. Traefik documents
[TCP-before-HTTP matching](https://doc.traefik.io/traefik/reference/routing-configuration/tcp/routing/rules-and-priority/)
and [TLS passthrough](https://doc.traefik.io/traefik/reference/routing-configuration/tcp/tls/).
These capabilities suggest a possible route; they do not prove the installed
configuration, certificate management, pinned WebRTC TURN SNI, UDP-blocked
connectivity or bounded allocation expiry. No wildcard catch-all or public
listener takeover is authorized by this proposal.

Before adopting or applying it, prepare and test exact-name routing, independent
inner authority TLS, dedicated certificate renewal, unrelated HTTPS regression
checks and rollback. Prove both WebRTC peers using TURN/TLS443 with client UDP
blocked, plus the60-second allocation authorization bound. DNS/certificate
ownership and bandwidth budget remain unspecified. Separate actual hosts are
still required for final recovery cutover evidence. Native, authentication and
hosting prerequisite implementation continues while public access stays closed.

## Selected hostname, 17 October 2026

The user selected `server1.helicoid.puriteum.com`; the earlier `barni.one`
suggestion is rejected and remains unused. Authenticated read-only Cloudflare
queries find `puriteum.com` active, with no exact selected-host record or
`*.helicoid.puriteum.com` / `*.server1.helicoid.puriteum.com` record. No DNS
record or VM configuration was changed. Certificate renewal, any separate TURN
name and the public routing design still require concrete verification.

A loopback bind alone does not make a public SNI/carrier route safe. The current
server router includes `/v1/bootstrap` and `/v2/bootstrap` and is exposed only
through the loopback development listener. Any future carrier must terminate at
an explicitly bounded authority surface that keeps bootstrap local-only or in a
verified SSH tunnel; forwarding the entire existing loopback router is not a
passed public-route gate. Inner TLS and independently enrolled deployment trust
remain required.

The isolated coturn connection/expiry work does not authorize public routing.
Integer-second last-fraction timing and result-existence-only aggregation were
rejected during coordinator review. An independent CA-verified socket probe
observes the patched relay forwarding a unique UDP packet0.4seconds after its
REST credential timestamp. That is a timestamp-cutoff failure, not a measured
60-second-from-issuance result. The hard renewable60-second lease and public
adoption remain Not Verified; the requirement is unchanged.
