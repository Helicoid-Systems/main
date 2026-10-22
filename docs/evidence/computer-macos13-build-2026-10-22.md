# Helicoid Computer macOS 13 build — 22 October 2026

The retained Swift Computer status app now builds from its own package manifest
with a macOS 13 deployment target. The legacy Swift Manager package remains at
macOS 15 while the shared Qt Manager is developed. The Computer package omits
the legacy Manager-only fixtures, overview model, and guided server setup model;
the shared host-check result and launch-flag parsing remain available to the
Computer build without importing those models.

Local verification on the available Apple Silicon macOS host:

- `xcrun swift build -c release --package-path Sources --scratch-path /private/tmp/hs-computer-macos13 --triple arm64-apple-macosx13.0 --product HelicoidComputer` passed.
- The same release package build passed for `x86_64-apple-macosx13.0` with scratch output under `/private/tmp/hs-computer-intel13`; `file` and `vtool -show-build` report x86_64 and `minos 13.0`.
- `HELICOID_BUILD_DIR=/private/tmp/hs-macos-full-build HELICOID_DIST_DIR=/private/tmp/hs-macos-full-dist ./scripts/build.sh` passed: 238 Core tests, 18 Manager tests, 14 Computer tests, both release package builds, and app packaging.
- `vtool -show-build` reports the Computer executable `minos 13.0`; its `Info.plist` reports `LSMinimumSystemVersion` 13.0. Manager reports 15.0 in both places.
- `codesign --verify --deep --strict` passed for both packaged apps. The Computer resource bundle is present and both supplied layered icons compiled.

The packaging script asks SwiftPM for each build's binary directory, then uses
the correct app-specific minimum for `Info.plist` and `actool`. CI now includes
an Intel macOS 13 Computer source build. The cross-build passed locally; the
Intel CI job has not run in this workspace. No runtime test on macOS 13 was
available, so installed behavior on macOS 13 remains unverified.

## Fresh source verification — 22 October 2026

After the permission-row and endpoint file-delivery changes, the complete Swift
test suite passed on the available Apple Silicon host: 242 HelicoidCore tests
and 15 HelicoidComputer tests. The latter includes permission prompt/recheck
coverage and a deterministic regression proving capture gate closure waits for
an in-flight frame callback, then prevents further delivery. The local HTTPS
fixtures required loopback access; the unprivileged sandbox denied their
socket binds, so the successful run used the approved loopback-enabled
execution environment.

The release build of the main macOS package passed, and a separate release
build of the Computer product passed with the explicit
`arm64-apple-macosx13.0` target. Swift tests also exposed an actor-isolation
issue in the lock-protected permission probe fixture, which is now marked
`nonisolated(unsafe)` around its lock-guarded state. A concurrent enrollment
test now asserts the protocol ordering and request counts without requiring an
unstable total ordering between independent requests.

The ScreenCaptureKit output path now serializes frame delivery against gate
closure. `stop()` waits for a callback already in progress, and once it returns
no later callback can forward a frame. This is a deterministic source-level
concurrency regression; it does not replace installed permission-revocation,
admission-expiry, or stream-stop acceptance.

Both release builds above were repeated after that gate change and completed
successfully, including the Computer package targeting macOS 13. The complete
test run after that change passed 242 Core tests, 18 Manager tests, and 15
Computer tests (275 total). The capture teardown regression was run again on its
own after adding an explicit synchronization point for the concurrent close
thread; it passed.

The packaging script also passed after the change, producing signed arm64
development bundles under
`/private/tmp/helicoid-macos-package-20261022-dist`. Strict ad-hoc signature
verification passed for both apps. `vtool -show-build` and `Info.plist` report
Manager minimum 15.0 and Computer minimum 13.0. Both packages contain their
compiled layered `Assets.car` and `AppIcon.icns`. This is bundle/build evidence,
not Developer ID signing, notarization, installation, or UI runtime acceptance.

These results prove source compilation and the stated unit/integration tests.
They do not prove installed capture, input, permission prompts on a desktop,
signing, or runtime behavior on macOS 13.
