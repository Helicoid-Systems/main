# WebRTC verified-channel gating — 2026-10-21

## Change

The ABI-3 bridge now requires a current, engine-issued verified-channel handle
before `hs_channel_send` or `hs_channel_poll` can expose product messages.
Inbound data received before the negotiated peer certificate is pinned is
dropped. Certificate changes, release of the active handle, and data-channel
close clear any queued inbound bytes, so they cannot become visible after a
later verification.

The native loopback and TURN/TLS harnesses now cover unverified send/receive
denial, dropping early inbound data, successful bidirectional messaging after
certificate pinning, and clearing queued data on repin. The callback-drain and
surviving-engine cases now establish verified peer bindings before sending.

This binds the data-channel API to the verified DTLS peer identity. It does not
implement server-issued endpoint admission, capture, media, or input.

## Verification

- `python3 modules/core/webrtc-native/scripts/check-header.py` passed; all 35
  bridge exports match the generated header.
- `python3 modules/core/webrtc-native/scripts/check-wrapper-contract.py`
  passed; all 32 implemented wrapper exports match the ABI contract.
- `git diff --check` passed for the bridge, harnesses, generated header, header
  generator, and package README.
- The local WebRTC source checkout was restored to its pinned commit from its
  retained Git object pack, including root `DEPS` and `.gn`. The packaged build
  still stops before GN: the separate `depot_tools` checkout has no usable Git
  metadata or commit objects, so its exact revision cannot be verified against
  the lock. No bypass was used. The modified native bridge and harnesses remain
  uncompiled and unrun; no native runtime pass is claimed for this change.

## Addendum: standalone binding encoder — 2026-10-22

The canonical byte encoder now lives in `native/channel_binding.h`, which is
used by the real bridge and can be compiled without the unavailable WebRTC
checkout. `native/channel_binding_test.cpp` checks the exact NUL-terminated
domain, fixed label length and bytes, big-endian stream ID, sorted digests,
maximum size, and invalid stream IDs.

Fresh checks:

- `c++ -std=c++17 -Wall -Wextra -Werror -pedantic -I modules/core/webrtc-native/native modules/core/webrtc-native/native/channel_binding_test.cpp -o /private/tmp/hs-webrtc-channel-binding-test` — passed.
- `/private/tmp/hs-webrtc-channel-binding-test` — passed.
- `cargo test --locked --manifest-path modules/core/Cargo.toml webrtc_binding::tests -- --nocapture` — 3 passed.
- The generated C header still matches 36 bridge exports; all 33 wrapper exports remain covered.
- The full WebRTC bridge and real-peer harness are still uncompiled and unrun because the pinned checkout lacks `src/build/dotfile_settings.gni`; this isolated codec proof does not establish native transport or production admission.

### Continuation: fixed-size binding buffer — 2026-10-22

The encoder now writes into a compile-time-sized array instead of allocating a
`std::vector` on the native C ABI path. It also rejects SCTP stream ID 65535,
which [RFC 8832 reserves](https://www.rfc-editor.org/rfc/rfc8832). The
standalone fixture was adapted to the fixed-size return type. The Rust decoder
now rejects that reserved stream ID both when constructing and decoding a
binding.

Fresh checks on the current source:

- The standalone C++17 binding fixture compiled with `-Wall -Wextra -Werror
  -pedantic` and ran successfully.
- `cargo fmt --check` passed for `modules/core`.
- `cargo test webrtc_binding::tests` passed all 4 focused tests.

The full WebRTC bridge and real-peer harness remain uncompiled and unrun
because the pinned WebRTC checkout is incomplete. These isolated checks do not
establish native transport, endpoint admission, capture, media, or input.

### Continuation: GN settings checkout diagnosis — 2026-10-22

At the time of this initial diagnosis, the root WebRTC `DEPS` pinned `src/build` at
`8f4d213ca23327067940282e2e896084844aa7a8`, and that exact upstream revision
contains `dotfile_settings.gni`
([pinned file](https://chromium.googlesource.com/chromium/src/build/+/8f4d213ca23327067940282e2e896084844aa7a8/dotfile_settings.gni)).
The local source has the `src/build` directory, but its `.git` metadata has no
usable HEAD or commit objects; Git therefore walks up to the WebRTC root and
cannot resolve the pinned build repository. The missing file is a consequence
of the incomplete DEPS checkout. The scratch depot_tools directory is also
missing usable Git metadata, and no system `gclient` was available. A direct
fetch attempt failed because DNS/network access was unavailable in that
attempt. No allowlist was synthesized and no unpinned build input was inserted.
The follow-up below records the later exact-revision fetch and dependency-sync
attempt.

### Continuation: exact subrepo recovery and sync resource limit — 2026-10-22

Fetched the exact `src/build` revision and `depot_tools` commit recorded in
`inventory/upstream-pins.json` into scratch. Their checked-out HEADs matched
`8f4d213ca23327067940282e2e896084844aa7a8` and
`2fd34a8813b6f34336aad40976cc4223768177f8`; the old empty placeholder
directories were preserved separately. The build script then passed its source,
tool, and DEPS pin checks and staged the existing native target. GN generation
still failed because `src/build/config/gclient_args.gni` is emitted by
`gclient` only after dependency processing completes. The old scratch checkout
also lacks `tools/clang/scripts/update.py`, so it cannot provide the recorded
LLVM package metadata.

Started the repository's clean pinned fetch/sync in a new scratch directory.
It reached 8.3 GiB while syncing `src/third_party/rust-toolchain`, with 2.6 GiB
left on the system volume. Stopped the sync at that point and removed only the
new partial checkout, restoring 11 GiB free. No GN args file was fabricated.
GN generation **Failed** at the missing generated file; native compilation and
the bridge harness were **Not Run**. The remaining prerequisite is a complete
dependency sync on a host with enough free disk for sync and build outputs. The failed
attempt's generated C header matches the current bridge exports and remains in
the working tree. Its transient blank LLVM package-version provenance value
was restored to the prior value.

Hardened `scripts/build-macos.sh` to require both generated sync inputs before
rewriting compiler provenance. `sh -n` passed. A repeat against the incomplete
scratch now exits with the missing `gclient_args.gni` path and the prescribed
fetch command; the existing `llvm_package_version` record remains intact.

### Continuation: dependency preflight — 2026-10-22

`scripts/fetch-upstream.sh` now checks scratch-volume free space before cloning
or syncing pinned dependencies and refuses to start below 16 GiB. This is a
protective floor based on the previous sync exceeding 8.3 GiB while incomplete;
it does not promise enough space for a complete sync or native build.
`scripts/build-macos.sh` checks the generated GN settings, pinned Clang update
script, and the libyuv and Abseil headers seen missing from the partial checkout
before regenerating the header or staging its GN target.

Fresh checks:

- `sh -n modules/core/webrtc-native/scripts/fetch-upstream.sh` — passed.
- `sh -n modules/core/webrtc-native/scripts/build-macos.sh` — passed.
- `git -C modules/core diff --check -- webrtc-native/scripts/build-macos.sh webrtc-native/scripts/fetch-upstream.sh` — passed.
- `modules/core/webrtc-native/scripts/fetch-upstream.sh /private/tmp/hs-finish-20261013/webrtc-upstream` — refused before Git fetch or `gclient sync`: 0.8 GiB was free against the 16 GiB floor.
- `modules/core/webrtc-native/scripts/build-macos.sh /private/tmp/hs-finish-20261013/webrtc-upstream out-arm64` — refused before header generation or GN staging and listed all four missing inputs: `build/config/gclient_args.gni`, `tools/clang/scripts/update.py`, `third_party/libyuv/include/libyuv/convert_argb.h`, and `third_party/abseil-cpp/absl/functional/any_invocable.h`.

No dependency sync, GN generation, native compilation, harness run, or live
preview was completed. The previous native and product acceptance boundaries
remain unchanged.

### Continuation: Windows build preflight — 2026-10-22

`modules/core/webrtc-native/scripts/build-windows.ps1` now gathers missing
gclient inputs, pinned GN/Ninja/Clang tools, build helpers, and Windows SDK
prerequisites before generating the ABI header or staging the GN target. The
SDK record must match the checked-in pin manifest, report a complete install,
and point to non-empty required SDK files. `Out` and `Cpu` are constrained
before they enter the GN output path and arguments. Both platform build scripts
also call the shared GN setup helper in `--check-only` mode first. The helper
checks exact source/depot_tools/DEPS pins and reads the source files plus root
GN patch context before any scratch-checkout writes.

Fresh checks:

- Python AST parse of `setup-gn-target.py` — passed.
- `sh -n modules/core/webrtc-native/scripts/build-macos.sh` — passed.
- `python3 modules/core/webrtc-native/scripts/setup-gn-target.py /private/tmp/hs-finish-20261013/webrtc-upstream --check-only` — passed for the exact pinned WebRTC, depot_tools, and DEPS revisions; reported that no files were changed.
- `git -C modules/core diff --check -- webrtc-native/scripts/build-windows.ps1 webrtc-native/scripts/build-macos.sh webrtc-native/scripts/setup-gn-target.py webrtc-native/README.md` — passed.
- `git diff --check -- docs/evidence/webrtc-verified-channel-gating-2026-10-21.md` — passed.

PowerShell is unavailable on this Mac host and only about 274 MiB is free, so
the Windows preflight/build was not executed. Windows compilation, the native
harness, and preview acceptance remain unverified.
