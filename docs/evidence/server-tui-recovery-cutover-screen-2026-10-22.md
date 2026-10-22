# Server TUI recovery cutover screen — 2026-10-22

## Implementation

The Bubble Tea server-management TUI now has a dedicated Recovery Cutover
screen. Its actions call the existing server-local `cutover status`, `drain`,
`approve`, and `cancel` commands, which retain their authenticated
security-administrator checks, independent fencing review, and exact typed
confirmation prompts. The TUI launches these commands with its active terminal
so prompts and operator output remain visible.

The screen states that drain pauses authority, restored deployments remain
quarantined until their separate recovery release, and local review cannot
coordinate two hosts, reconcile peer trust, or independently prove predecessor
fencing. The Updates and rollback screen now reports the lifecycle backend's
actual update, rollback, and backup/restore capabilities and their specific
unavailability reason instead of presenting the whole capability map as a
generic missing workflow.

## Verification boundary

`tui.go` was formatted with `gofmt`. With dependencies resolved from the
existing offline module cache and build outputs redirected to writable temporary
directories, `go test ./...` passed, including focused checks for cutover key
routing, screen copy, and mutation reconciliation. Native macOS-host and
`GOOS=linux GOARCH=amd64` builds both passed in `modules/server/tui`. No live
TUI interaction or server cutover was run. The screen only exposes existing
server-local cutover operations; it does not add two-host orchestration or
complete deployment recovery acceptance.

The Rust lifecycle capability reason map and regression were then checked with
`CARGO_INCREMENTAL=0 CARGO_TARGET_DIR=/private/tmp/helicoid-server-lifecycle-tests cargo test --offline --test installer`:
all 58 installer tests passed, including the regression that distinguishes
unavailable online update, backup/restore, and cutover operations. This
validates the server-local capability report; it does not establish cross-host
fencing or end-to-end recovery.
