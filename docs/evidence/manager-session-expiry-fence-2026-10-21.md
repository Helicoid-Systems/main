# Manager session expiry fence — 2026-10-21

## Change

`ServerSessionRegistry::session`, `currentStamp`, and `isCurrent` now reject
sessions at or after their Unix expiry. Their deterministic timestamp overloads
support boundary tests. The manager's one-second cleanup timer checks expiry
through `isExpired`, then invalidates the affected deployment and clears its
inventory state as before.

The registry tests cover the exact expiry second, the immediately preceding
second, stale callback rejection, and the guarantee that another deployment's
session stays current.

`DeploymentRegistry` also tags availability and trust-verification requests
with a profile incarnation. Removing and re-adding a saved profile under the
same local ID therefore cannot make a callback from the removed profile
current again. This addition has compile-only verification below; the existing
registry test result above predates it and does not prove this specific case.

## Verification

- `cmake -S apps/manager -B /private/tmp/helicoid-manager-registry-expiry -DHELICOID_MANAGER_REGISTRY_TESTS_ONLY=ON -DBUILD_TESTING=ON` configured successfully.
- `cmake --build /private/tmp/helicoid-manager-registry-expiry --target manager_registry_tests -j 4` passed.
- `ctest --test-dir /private/tmp/helicoid-manager-registry-expiry --output-on-failure` passed: 1/1 CTest target.
- The full Qt Manager configure is not verified: CMake could not find the required Qt 6.10 package in this environment. No GUI build or runtime claim is made.
- `deployment_registry.cpp` compiled independently with C++20, `-Wall -Wextra -Werror -pedantic`; this confirms compilation only and does not verify callback behavior.
