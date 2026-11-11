# Manager read-only SSH host preflight — 2026-10-22

The shared Qt Manager's Server Setup workspace now offers a read-only `Check
Host` action for native systemd and Docker Compose deployments. It validates
`[user@]host[:port]` locally, launches OpenSSH with batch authentication and
`StrictHostKeyChecking=yes`, disables host-key updates and connection
multiplexing, and sends a fixed Python preflight script over stdin. The
configured data path is restricted to the normalized Helicoid data root and
encoded as hex inside that script; it is not interpolated into an SSH remote
command. Output is bounded at 64 KiB and parsed against a versioned JSON
shape.

The report includes the verified SSH fingerprint, Ubuntu 24.04/26.04 x86-64,
systemd and noninteractive sudo checks, free space on the nearest existing
ancestor of the selected data path, whether the selected data path already
exists, the configured TCP authority port, and an existing `/opt/helicoid`
installation. Compose mode also checks the root-owned Docker daemon endpoint
and requires Compose 2.17.0 or newer, matching the lifecycle backend. These
commands are fixed, bounded, and read-only. For LAN setup it also checks
whether the configured private bind address is assigned to a host interface.
When `/opt/helicoid/bin/hs-server` and its parent directories are root-owned,
non-writable by group/other, and not symlinks, the same SSH check also queries
`installer capabilities-v2`, `configuration-v2`, and `status-v2` through
noninteractive sudo. This reports the backend's supported lifecycle actions,
the durable operation state, and the explicit unavailability reason for signed
updates/rollback and deployment backup/restore. A missing backend, denied sudo,
or malformed status stays unavailable without failing host preflight. Remote
strings are bounded and sanitized before display. This starts read-only
lifecycle visibility; remote install and mutation still remain unavailable.
The complete SSH operation has a 45-second bound to account for the extra
read-only lifecycle commands on an installed host.
This workflow does not install files, stop or start services, change host
keys, inspect TLS files, or establish deployment identity. Remote install/apply
remains unavailable.

The existing Mac setup preflight now accepts the same Ubuntu 24.04/26.04
releases as the Linux lifecycle backend; a regression covers both supported
releases and an unsupported release.

## Verification

- `ssh -G -p 22 -- localhost` and `ssh -G -- ::1` confirmed that the local
  OpenSSH accepts the fixed `--` argument boundary and IPv6 destination syntax.
  These are local configuration probes; no remote host was contacted.
- `git diff --check` passed, and a trailing-whitespace scan of the changed
  source and documentation returned no matches.
- The updated Manager pure C++ harness passed all 3 targets: registry
  isolation, exact preview metrics, and native runtime loader boundaries.
- The embedded Python script parses after substituting both deployment modes
  and empty or populated bind-address values. The lifecycle-query addition
  also parses in all four variants. This checks script syntax only; it does not
  exercise the remote commands.
- CMake configuration stopped at `find_package(Qt6 6.10...<6.11 ...)` because
  Qt 6.10 is not installed. The new C++ Manager source was not compiled or
  observed onscreen.
- `swift test --filter SSHSetupTests` could not compile its manifest: the
  sandbox denied Swift's attempt to write its module cache under the user's
  cache directory. A retry with build/cache paths in `/private/tmp` hit the
  same sandbox restriction. The regression is added but remains unverified.
- No target SSH host check was run.
- No SSH host was contacted; remote lifecycle status and capabilities remain
  unverified on an installed host. Qt 6.10 is unavailable here, so the modified
  Manager has not been compiled or exercised.
