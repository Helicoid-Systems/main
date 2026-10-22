# Manager viewer preview request and authority fencing — 2026-10-22

Opening an authorized computer viewer now starts the existing short-lived
observation-grant flow automatically. Viewer creation already requires a
current signed-in session, verified deployment authority, enrolled endpoint,
and a current account observe scope. The grant request repeats those checks;
the server issues the signed grant and the endpoint must redeem it before the
native viewer admits frames. The preview label expands into the viewer's
available content area, where an admitted frame is fitted without changing its
aspect ratio. A manual retry button remains available after a failed attempt.
Closing the viewer still owns peer teardown.

The asynchronous grant, signaling, and frame-session callbacks now retain and
recheck the trusted deployment ID, grant-signing key, and authority generation.
They also stop if that profile enters the authority-mismatch state. This fences
in-flight preview work when a saved server profile changes or a server's
authority checkpoint is rejected.

The Manager now checks the bundled native runtime's observation capability
before requesting a server grant. A missing bridge or a bridge that does not
advertise observation returns a local unavailable status, so the endpoint is
not awakened and no short-lived server grant is issued for a receiver that
cannot display frames.

This aligns the viewer-open behavior with the core v1 console plan. It does not
enable observation capability or weaken the native engine gate. The server and
endpoint continue to refuse observation while the required native capability
bits are disabled, and no actual screen pixels are claimed.

## Verification

- Source inspection confirmed the request is scheduled only after the viewer's
  existing verified-session, enrollment, and observe-scope checks pass.
- The scheduled callback uses the viewer as its Qt context and guarded widget
  references, so closing the viewer before dispatch prevents the request.
- The preview callback chain rechecks the pinned deployment/key/generation
  tuple and authority-mismatch state before accepting grants or signaling.
- Focused trailing-whitespace scan of the added capability preflight
  (`awk` over lines 11739–11743 of `src/main_window.cpp`) passed.
- `cmake --preset manager-macos-arm64-release` could not start because Ninja is
  not installed. `cmake -S . -B /private/tmp/helicoid-manager-make-config -G 'Unix Makefiles'`
  reached compiler detection, then stopped because Qt 6.10 was not installed.
  The C++ change was not compiled or exercised onscreen.
- Installed preview, endpoint admission, expiry, revocation, and desktop-change
  acceptance remain Not Verified.
