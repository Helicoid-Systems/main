# Shared Manager cross-platform source build checkpoint — 2026-10-22

## macOS Qt Manager

- Installed Qt 6.10.3 `qtbase` in `/private/tmp/helicoid-qt-6.10.3` and configured the independent Manager target with CMake 4.4.3, arm64, and `CMAKE_OSX_DEPLOYMENT_TARGET=13.0`.
- `cmake --build /private/tmp/helicoid-manager-qt6103 --target HelicoidManager --parallel 1` completed successfully. It compiled and linked the full Qt Widgets Manager source against the pinned SARibbon revision. Non-fatal upstream/Qt deprecation warnings remain.
- Corrected the top-level WebRTC include path, C++/Qt type and scoped-ID errors, async preview captures, QObject child traversal, and mutable SSH preflight callback declarations exposed by that build.
- The packaging script now uses the configured deployment target for `actool`. The packaged arm64 binary records minimum macOS 13.0. The app package includes the Qt 6.10.3 frameworks and plugins and the verified `.icns` fallback.
- CPack's DragNDrop path could not create a DMG because `hdiutil` returned `Device not configured`. `cpack -G ZIP` produced `/private/tmp/helicoid-manager-macos-arm64-dev.zip`, 22 MiB, SHA-256 `5fb2299a264dd2d8b21772663f195fa89ee304d373fafb5f7e48083ead6e5d5f`.
- Xcode `actool` could not open the layered icon source in this restricted run, so the normal hash-verified fallback was used. The CI-required layered icon path is not verified by this package.
- This is an unsigned development package and does not contain `libhs_webrtc_shared.dylib`. Native observation remains unavailable; the package does not demonstrate a live preview or installed UI behavior.

## macOS Swift products

From `apps/macos`, `CLANG_MODULE_CACHE_PATH=/private/tmp/helicoid-clang-module-cache swift build --disable-sandbox --scratch-path /private/tmp/helicoid-swift-build-current` completed successfully for `HelicoidCore`, `HelicoidManager-product`, and `HelicoidComputer-product`. One existing `onChange(of:perform:)` deprecation warning remains. This is a source build, not a runtime or permission-prompt check.

## Windows endpoint source

Installed .NET SDK 10.0.402 in `/private/tmp/helicoid-dotnet10`. The following no-restore build completed in 64.35 seconds with zero warnings and zero errors:

```sh
dotnet build apps/windows/src/Helicoid.WindowsPlatform/Helicoid.WindowsPlatform.csproj \
  --no-restore --configuration Release -m:1 /nodeReuse:false --verbosity minimal
```

The build compiled `Helicoid.Domain`, `Helicoid.WorkerTransport`, and
`Helicoid.WindowsPlatform`, including the endpoint grant verifier, admission
lease, WebRTC wrapper, and bounded capture integration. It ran on macOS and is
not a Windows OS/runtime proof.

The `Helicoid.Computer` WinUI app could not be compiled: its `obj/project.assets.json`
was absent (`NETSDK1004`), and restoring its pinned Windows App SDK dependencies
exhausted the remaining temporary disk space. The app's `MainWindow.xaml.cs`
integration, Windows native DLL, Windows installation, and actual observation
remain unverified. The task-created .NET/NuGet restore caches were removed to
recover 1.2 GiB; the source tree and the Manager ZIP were preserved.

## Rust coordination server

The server and shared core sources passed a no-link compile check with the
existing locked, offline dependency cache:

```sh
CARGO_PROFILE_DEV_DEBUG=0 CARGO_PROFILE_DEV_INCREMENTAL=false \
  cargo check --locked --offline --manifest-path modules/server/Cargo.toml \
  --bin hs-server
```

Cargo reported `Finished dev profile [unoptimized] target(s) in 19.45s`. A full
`cargo build` compiled the dependencies and both `hs-core` and `hs-server`, but
the final macOS executable link failed with `No space left on device`; its
1.5 GiB temporary target cache was removed. The successful `cargo check` does
not produce a deployable server binary, package, or installed deployment.

## Go server-management TUI

From `modules/server/tui`, the following command completed successfully with
Go 1.27.1:

```sh
GOCACHE=/private/tmp/helicoid-go-cache \
GOMODCACHE=/Users/barni/go/pkg/mod GOWORK=off GOTOOLCHAIN=local \
  go build -mod=readonly -o /private/tmp/helicoid-server-tui-20261022 .
```

The output is a macOS arm64 binary kept outside the repository. This compiles
the TUI; it does not demonstrate SSH behavior or installed Linux
administration.

No test suites were run in this checkpoint. Windows-to-Mac pixels, grant
denial/expiry/revocation, desktop-change teardown, signing, notarization, and
release acceptance remain open.
