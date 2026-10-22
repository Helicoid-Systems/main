# Manager bundled native loader check — 2026-10-22

## Change

The macOS Manager loader resolves
`Contents/Frameworks/libhs_webrtc_shared.dylib`. The Manager CMake install rule
now stages the architecture-matched native library at that location, and the
DMG workflow checks the packaged Manager and library architectures and the
library SHA-256. The workflow also copies the package library into a temporary
test bundle and runs the existing native runtime test binary in `--bundled`
mode so the production bundle lookup and engine init/shutdown are exercised.

## Local evidence

- Compiled `native_webrtc_runtime.cpp` and `native_webrtc_runtime_tests.cpp`
  with Clang C++20, `-Wall -Wextra -Werror -pedantic`, and
  `HELICOID_MANAGER_NATIVE_WEBRTC_TESTING`.
- Built `native_webrtc_abi_fake.cpp` as a macOS dynamic library with
  `HS_TEST_ENGINE_INIT_SUCCESS` and placed it at the expected Frameworks path
  in a temporary `.app` layout.
- Ran the compiled runtime test binary from that bundle with `--bundled`; it
  loaded the library, checked connection support and the disabled observation
  capability, initialized the engine, and shut it down successfully.
- Parsed both affected GitHub Actions workflows and checked their embedded
  Bash blocks with `bash -n`.

## Boundary

The local dylib was a test fixture, not the pinned WebRTC build. This does not
prove the Qt Manager package, actual WebRTC dylib loading, Windows endpoint
capture, delivered pixels, or installed desktop behavior. Those checks are
configured in the architecture-specific CI and still require successful
runner and lab results. Native observation capabilities remain disabled.
