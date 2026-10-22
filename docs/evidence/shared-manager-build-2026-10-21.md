# Shared Manager build checkpoint

## Verified source builds

- `CLANG_MODULE_CACHE_PATH=/private/tmp/helicoid-clang-module-cache swift build --disable-sandbox --scratch-path /private/tmp/helicoid-swift-build-current` completed successfully. It built `HelicoidCore`, `HelicoidManager-product`, and `HelicoidComputer-product`.
- `CARGO_TARGET_DIR=/private/tmp/helicoid-manager-auth-target cargo build --locked --offline --manifest-path native-auth/Cargo.toml` completed successfully for the Qt Manager's Ed25519 signing helper.
- CMake 4.4.3 parsed `CMakePresets.json` and listed the two macOS architecture presets on this host; the Windows preset is host-gated. The presets pin the Qt 6.10 line and separate build directories by target.
- No tests were run in this checkpoint.

## Build limits

- CMake requests the Qt 6.10 compatibility line (`6.10...<6.11`). Configuring at `/private/tmp/helicoid-manager-config-check` stopped at `find_package` because no Qt 6 SDK is installed in this workspace. The C++ Manager, SARibbon integration, key-store adapters, and native-auth link have therefore not been compiled together.
- `dotnet build src/Helicoid.WindowsPlatform/Helicoid.WindowsPlatform.csproj --no-restore -m:1 /nodeReuse:false` could not start because `dotnet` is not installed. No Windows source build is claimed.
- These are compile checks only. They do not demonstrate the Manager UI, secure-store behavior in an installed app, endpoint admission, capture, pixels, or control.

## Qt package target source delta — 2026-10-22

The Manager CMake project now installs the Qt runtime dependencies through the
platform deployment tool and exposes CPack packages: a macOS DMG and a Windows
x64 ZIP. The README uses CPack's `package` target for Ninja and `PACKAGE` for
Visual Studio. The pinned SARibbon license is copied into the package from the
fetched source. Packages are explicitly labeled unsigned development packages;
signing, notarization, and the configured Qt kit's applicable license notices
remain release gates.

This source delta was not configured or packaged on this host. The Qt 6.10
package configuration remains unavailable as recorded above, so neither
deployment-tool invocation nor the generated DMG/ZIP is verified.

## Native Qt CI workflow source delta — 2026-10-22

The integration repository now has a path-filtered GitHub Actions workflow for
native macOS arm64, macOS x86_64, and Windows x64 builds. Each job installs Qt
6.10.0 and Rust 1.99.0, configures the corresponding CMake preset, builds the
Manager, creates an unsigned development package, and retains that package as a
14-day artifact. macOS jobs ensure Ninja is available. Workflow permissions
are read-only for repository contents.

Local YAML parsing and CMake preset listing succeeded. The workflow has not run
on GitHub, and no platform package artifact or native Qt compilation is
claimed. Signing, notarization, installed runtime behavior,
and release acceptance remain separate gates.

## Server health details source delta — 2026-10-22

The Server page now retains a bounded, timestamped `/health` summary for the
selected profile after a successful availability check. It displays the API
health state, authorized-operation state, storage schema, bootstrap state,
and boolean capabilities when the response reports them. The UI labels this
as a public health report and keeps identity verification and installation
state separate. A new check clears the previous summary; a failed check does
not leave stale details visible. Source inspection also found and fixed a
missing profile-to-deployment lookup in the selected-server detail renderer.

This delta is not compiled or visually verified because the Qt 6.10 SDK is
unavailable in this workspace. No tests were run.

## macOS Manager icon source integration — 2026-10-22

The Qt macOS package step now invokes Xcode `actool` on the preserved
four-file `HS Manager App icon.icon` source and installs the resulting
`Assets.car` and `Manager.icns` into the app bundle without changing the
macOS 13 target. Each source file is SHA-256 checked against the retained brand
manifest. Local packaging can fall back only to the checked-in icon whose hash
is verified against its provenance file. Both macOS CI package jobs set
`HELICOID_MANAGER_REQUIRE_LAYERED_ICON=1`, so a fallback cannot mask a layered
compilation failure there.

Xcode 27.0 successfully compiled the supplied icon to a 2,904,232-byte
`Assets.car` and a 90,955-byte `.icns`. The generated install script was
configured and run against a temporary app bundle: it copied both artifacts,
set `CFBundleIconName`, and preserved `CFBundleIconFile`. The generated `.icns`
SHA-256 matches the approved fallback. A second run exercised fallback cleanup:
it removed stale `Assets.car` and icon-name metadata, checked the fallback hash,
and installed the `.icns`.

The full Qt target and DMG were not built because Qt 6.10 is not installed in
this workspace. The temporary install-script harness used a no-op stand-in for
`macdeployqt`; it verifies the icon integration and fallback logic only, not
the complete packaged application or Finder/Dock behavior.

## Qt 6.10 arm64 compile and package checkpoint — 2026-10-22

The earlier missing-SDK result is superseded by this checkpoint. Qt 6.10.3
`qtbase` was installed under `/private/tmp/helicoid-qt-6.10.3`; no SDK files
were added to the repository. With CMake 4.4.3, SARibbon pinned at
`806e3e93be4dd7676697d3017282a4359519e053`, arm64, and a 13.0 deployment
target, the Qt project configured and `HelicoidManager` compiled and linked.
This compile uncovered and fixed the native WebRTC include-directory scope,
several C++/Qt type mismatches, scoped ID field names, asynchronous preview
captures, QObject child discovery, and stateful SSH-preflight callback
constness. Existing deprecation warnings remain non-fatal.

The icon build script now passes the configured macOS deployment target to
`actool` instead of hard-coding 15.0. The app binary records `LC_BUILD_VERSION`
minimum OS 13.0 and is arm64. `macdeployqt` bundled Qt 6.10.3 frameworks and
plugins; CPack's ZIP override produced a 22 MiB development bundle at
`/private/tmp/helicoid-manager-macos-arm64-dev.zip`
(SHA-256 `5fb2299a264dd2d8b21772663f195fa89ee304d373fafb5f7e48083ead6e5d5f`).
The package contains the provenance-checked `Manager.icns` fallback. Xcode
`actool` could not open the supplied layered icon in this restricted run, so
the CI-required layered-icon path remains unverified here.

The configured DragNDrop package target reached CPack but `hdiutil` failed with
`Device not configured`; therefore no DMG was produced. The ZIP does not
contain `libhs_webrtc_shared.dylib`, is an unsigned development artifact, and
does not activate native observation. No tests or installed-UI/runtime checks
were run; Windows compilation, signing, notarization, and the actual
Windows-to-Mac preview remain unverified.
