# Manager visible overview previews — 2026-10-22

## Implementation

The Computers grid now creates preview tiles only for entries intersecting the
visible viewport. Automatic requests require a current signed-in deployment,
verified authority, an enrolled and included computer, and current `observe`
scope. The existing grant verifier, certificate-attested signaling, native
channel proof, admission checks, frame bounds, and lease renewal remain the
only path to pixels.

One shared Manager limit covers overview and separate-viewer requests. It is
currently eight sessions, matching the server's per-endpoint pending-grant
limit while remaining a conservative global cap until performance measurements
support a larger budget. Unredeemed requests hold a cooldown reservation until
their short grant expires. A first received frame marks the grant redeemed;
the active preview then occupies the budget through its label-owned lifecycle.

Scrolling a tile out of view, leaving the Computers grid, losing local observe
scope, or opening that computer's separate viewer stops and releases the
overview peer. Re-entering the viewport starts a fresh authorized request.
Stale observe scope is refreshed with a bounded retry delay. The tile exposes a
retry action after a failed request. If the bundled native runtime does not
advertise observation, the Manager stays fail-closed and requests no server
grant.

The Computers ribbon and native View menu offer Compact 192, Standard 256,
Large 320, and Extra Large 448 overview cards. Changing the size updates grid
cells and visible tile dimensions, stops existing overview peers so they can
be recreated at the new size, and saves the choice in the current signed-in
account and deployment scope. Schema-v2 migration bundles export each scoped
Grid/List and size preference; import validates both values, previews scoped
display choices for confirmation, restores settings on write failure, and
accepts older bundles that omit these fields. The earlier development format's
top-level global size is also accepted during import.

Standard is the default. Its grid cell is 272 × 258, tile is 256 × 246, and
preview area is 144 pixels high. Compact uses a 208 × 222 cell, 192 × 210 tile,
and 108-pixel preview area. Large uses 336 × 294, 320 × 282, and 180 pixels;
Extra Large uses 464 × 366, 448 × 354, and 252 pixels, respectively.

The four preset records now live in the standard-library-only
`computer_preview_metrics.h` contract. The Qt window converts those dimensions
to `QSize`; pure C++ tests pin every cell, tile, preview, and details dimension
and the standard fallback for an unknown index.

## Verification

- Configured the dependency-free Manager targets with Unix Makefiles in
  `/private/tmp/helicoid-manager-pure-tests`.
- Built `manager_preview_metrics_tests`, `manager_registry_tests`, and
  `manager_webrtc_runtime_tests` with the configured strict compiler flags.
- `ctest --test-dir /private/tmp/helicoid-manager-pure-tests
  --output-on-failure` passed: 3/3 targets, including exact preview dimensions.
- A full Manager configure in
  `/private/tmp/helicoid-manager-qt-current` stopped at
  `find_package(Qt6 6.10...<6.11)`: Qt 6.10 is not installed. The new Qt
  scheduler, sizing, migration, and widget code are therefore not compiled or
  exercised onscreen.

## Acceptance boundary

The existing native bridge advertises connection capabilities only; capture,
admission, and media capability bits remain disabled. The Windows native DLL is
absent from this checkout's retained inventory, and the .NET SDK is unavailable
in this environment. No Windows-to-Mac frame delivery, permission denial,
grant expiry/revocation, desktop-change teardown, installed UI behavior, or
performance measurement is established by this change.

## Test-target build correction — 2026-10-22

The registry test was registered with CTest but marked `EXCLUDE_FROM_ALL`, so
the default build left CTest without its executable. The target now builds by
default. A fresh tests-only configure, default build, and CTest run passed all
three targets: registry, preview metrics, and native runtime. This does not
change the Qt 6.10 or installed-UI acceptance boundary above.
