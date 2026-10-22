---
status: proposed
date: 2026-10-09
---

# Coordination server with bounded endpoint authority

For the LAN-first, managed-computer scope, propose one small organization-operated server for identity, enrollment, direct computer access, policy, and audit metadata. Endpoints validate short-lived, peer-bound grants; media normally travels directly or through an optional authorized relay. Bounded authority limits the impact of operator/server failure. Admission, independent observation sessions, unique control ownership, offline expiry, and platform recovery need real-device proof. The server has no room, class, or lesson requirement. The historical filename is retained so existing links remain valid.

The [active-authority contract](../spec/contracts.md#active-server-authority-and-cutover), expanded on 10 October 2026, requires one active issuer, independently approved fresh-identity cutover, actual old-host fencing, and separately persisted peer adoption. Version 1 does not promise active-active failover or universal detection of identical-secret server copies. Required persistent-state failure blocks new grants/effects while safety teardown remains available. These are proposed engineering requirements awaiting E02/E07 and V47/V49 proof, not implemented guarantees.
