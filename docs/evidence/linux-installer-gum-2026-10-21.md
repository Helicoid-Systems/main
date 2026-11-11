# Linux installer Gum interaction

## Implemented

- When Gum is installed, the first-run installer opens a directory picker for the extracted signed package.
- Network scope uses a two-choice Gum selector for LAN or loopback development; the plain-terminal input remains as fallback.
- The reviewed deployment summary uses a Gum table before the full signed plan is printed.

## Verification

- `bash -n modules/server/scripts/install-linux.sh`: passed.
- The installed Gum `choose`, `file`, and `table` help output confirms the used flags exist.
- The actual `gum choose` command ran in a terminal session and returned the selected `LAN` value.
- Piping representative configuration rows through the exact `gum table` invocation rendered a single-headed table correctly.
- `shellcheck` is not installed in this environment.

## Limits

The interactive picker and confirmation path were not driven in a terminal session. This change does not add Compose selection, online release discovery, or prove that the installer completed on Ubuntu.
