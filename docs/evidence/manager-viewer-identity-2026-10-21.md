# Manager deployment-scoped access registry checkpoint — 21 October 2026

The application-level viewer registry now keys each window with the typed
`ScopedComputerId` value (authenticated deployment identity plus server-local
computer ID). Looking up an existing viewer, disconnect teardown, and inventory
retention use those fields directly, so duplicate computer IDs on two servers
do not share a viewer entry.
The scoped identity constructors also reject empty, oversized, or ASCII-control
server-local IDs before they can enter application registries.

Observe access snapshots are now stored by authenticated deployment session
stamp in a separately testable registry. A stale session cannot reuse its
scope, a late expiry callback cannot erase a replacement session's scope, and
expiry or revocation on one server leaves another server's permissions intact.

## Evidence

- `cmake --build /private/tmp/helicoid-manager-registry-build --target manager_registry_tests`
  — passed with AppleClang.
- `ctest --test-dir /private/tmp/helicoid-manager-registry-build --output-on-failure`
  — passed, 1/1 test target.
- The focused registry assertion stores the same server-local computer ID under
  two deployments and verifies that both map entries remain distinct.
- The observe-scope assertion covers different permissions for the same
  computer ID on two servers, per-server expiry, session replacement, and stale
  expiry callbacks.
- Whitespace and final-newline checks passed for the five source files and this
  evidence record.

## Limits

This is source and standard-library registry evidence. The Qt/SARibbon target
was not compiled or run in this environment because Qt 6.10 is unavailable.
The viewer remains an explicit unavailable-preview placeholder until endpoint
admission and live media transport are implemented.
