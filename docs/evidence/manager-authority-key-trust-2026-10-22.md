# Manager deployment authority pin — 2026-10-22

## Change

The Manager's local per-profile trust now pairs the deployment ID and server's
Ed25519 grant-verification public key with its durable authority generation.
QSettings saves and restores the generation with the ID and key before the
registry is populated; profile import/export excludes all three trust values.
Identity verification fetches
`GET /v2/deployment/authority` and requires the operator to compare the
deployment ID, the SHA-256 fingerprint of the decoded 32-byte public key, and
the numeric generation against a separate organization channel. A generation
rollback or same-generation key replacement is rejected by the registry; a
higher generation requires deliberate verification. Sign-in, bootstrap,
identity confirmation, guarded server actions, and logout compare the full
tuple where those paths already perform a server-identity guard. A detected
mismatch marks the server profile in the running Manager, immediately
invalidates its local session and inventory, and stays visible until an
offline-confirmed pin is saved. Forgetting a profile removes all local
authority pins.

This is local trust and mismatch detection only. The server publishes the
persisted generation. The Manager does not report which remote peers adopted a
replacement authority. This does not define or issue a signed admission grant,
bind the key to the TLS peer, enable media, or prove capture/input. The frozen
endpoint-admission/grant protocol remains a separate prerequisite.

## Verification

- `cmake --build /private/tmp/hel-manager-registry-build --target manager_registry_tests` passed after adding monotonic rollback and legacy-settings-restore cases.
- `ctest --test-dir /private/tmp/hel-manager-registry-build --output-on-failure -R manager_registry_tests` passed, 1/1 target.
- `cargo fmt --manifest-path modules/server/Cargo.toml -- --check` passed.
- `cargo test --locked --offline --manifest-path modules/server/Cargo.toml --test http_api` passed, 18 tests including the public authority generation response.
- `cargo test --locked --offline --manifest-path modules/server/Cargo.toml --lib` passed, 95 tests including restore authority generation.
- Full Qt Manager configuration at `/private/tmp/hs-manager-authority-ui-20261022` stopped because Qt 6.10 is not installed. The authority UI and QSettings persistence are therefore source-reviewed only; no full Manager build or runtime test is claimed.
- Authority-generation QSettings reads now happen during profile loading, and writes happen with profile trust saves. Computer-inclusion saves no longer touch authority trust state. This source correction has not run through the unavailable Qt build.
