# Manager profile migration bundle

## Implemented source path

The shared Qt Manager exports `helicoid.manager.migration` schema version 2
with connection profiles, bounded deployment/account-scoped monitoring
inclusion preferences, and the global Grid/List computer view. The importer
accepts the previous schema-version-1 profile-only document and schema-v2
bundles with or without the optional `computer_view` field. Version 2 rejects
unknown root and preference fields, excessive scope or computer counts,
malformed scope hashes, invalid IDs, and duplicate computer selections.

Import is explicit through File > Import Profiles & Preferences. The review
dialog names the new profiles, states how many preference scopes will be
replaced, and previews a bundled Grid/List change. New profiles do not carry
deployment trust and must be verified; matching local profiles retain their
existing local pins. Preferences remain local display/inclusion state and do
not authorize server operations. The view selection is persisted in application
settings and restored at startup.
QSettings persistence is checked before the in-memory preference cache changes;
failed saves roll back the profiles and preference values on a best-effort
basis.

The bundle excludes passwords, bearer tokens, MFA recovery material, server
trust pins, sessions, and Manager signing keys. The setup draft is excluded
because host addresses and filesystem paths are machine-specific. The legacy
Managers do not persist reusable password or bearer-token credentials. On
macOS, both the previous Swift Manager and Qt Manager use the same Keychain
service/account. Qt reads the Swift Manager's versioned seed directly and
upgrades the earlier Qt raw 32-byte item in place to that shared versioned
format, preserving the existing device identity without exporting key
material. On Windows, the inspected predecessor has no matching persisted
signing-key adapter; the Qt Manager uses Credential Manager and a newly
generated identity may need normal approval on each server. A migration bundle
never moves private keys between machines or operating systems.

## Evidence boundary

This records a source implementation only. Qt 6.10 is unavailable in the
current workspace, so the desktop target and interactive import/export flow
were not built or exercised here. This is not Windows/macOS migration
acceptance. Existing schema-v1 imports remain supported at source level.
