# Manager file-job cancellation fence — 2026-10-21

## Change

Cancellation now verifies that the selected table row's deployment matches
the profile's authenticated deployment, and names the server in the exact-ID
confirmation. After the modal dialog returns, it rechecks the profile mapping,
session generation, expiry, and loaded job state using the scoped deployment
and job ID before sending the request. The asynchronous response is also tied
to the same session generation and server origin. A row refreshed or moved to
a terminal state while the dialog was open cannot authorize cancellation.

## Verification

- Source inspection confirms the request uses the deployment and session
  revalidated after confirmation, and only a current `uploading` or `queued`
  row proceeds.
- A whitespace scan reported no trailing whitespace in the touched C++ files.
- The Qt Manager source is not compiled or runtime-verified because Qt 6.10 is
  unavailable in this environment.
