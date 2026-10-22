# Exact confirmation phrases in the server TUI

## Change

Destructive and authority-changing TUI prompts now compare the entered value
exactly after the terminal reader removes the line ending. Leading or trailing
spaces no longer count as an exact ID, phrase, or acknowledgment. Free-form
fields such as evidence references continue to be trimmed before submission.

This applies to authority drain, approval and cancellation; restore-quarantine
release and both recovery acknowledgments; and the existing account, Manager,
group, enrollment, access, and file-workflow confirmations.

## Verification

- `gofmt` completed on the touched TUI source files.
- `GOCACHE=/private/tmp/hel-go-cache go test -mod=readonly ./...` passed.
- Interactive terminal behavior has not been manually exercised in this
  environment.
