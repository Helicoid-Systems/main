# Helicoid Manager

The shared C++20/Qt Widgets manager is independently configured from this
directory. It provides the persistent console shell, local connection-profile
storage, server availability checks, v2 device-key challenge sign-in with
conditional TOTP, per-deployment in-memory sessions, and authorized metadata
reads for computers, manager devices, accounts, and access assignments. The
Server ribbon can perform the first-administrator v2 bootstrap by proving the
Manager's protected device key against the deployment identity and single-use
bootstrap token, then show the first administrator's MFA setup material once.
MFA confirmation is a separate action so the administrator can store recovery
codes before activation.
Computer records have combined Grid/List views, search, server filtering,
enrollment-state filtering, and a deployment-scoped group filter populated only
from groups attached to already-authorized inventory records. A collapsible
Computers panel stores its visibility and All or explicit inclusion per
deployment and signed-in account; it restores explicit IDs only while they
remain in that account's authorized inventory. Inclusion only controls which
authorized records appear in the overview. Visible enrolled tiles start a
preview request only after current observe scope is confirmed; the server
grant and endpoint admission remain separate requirements. The Computers
ribbon and native View menu provide Compact 192, Standard 256, Large 320, and
Extra Large 448 preview cards. Size is saved locally for the signed-in account
and server scope and affects only the overview layout.
The separate viewer offers a local PNG screenshot only while a recently received,
server-granted frame is current. It rechecks frame freshness and grant expiry
after the save dialog returns, writes atomically to the chosen path, and clears
the retained frame when observation ends. This source path is not yet built or
runtime-accepted.
The Enrollment
and Groups dialog selects one owning deployment and supports revisioned group
changes, one-time endpoint invitations, invitation revocation, proof-backed
endpoint approval, and epoch-fenced endpoint revocation. Invitation codes are
shown once and are never listed. Mutating enrollment actions require sign-in
MFA from the last 120 seconds and exact ID confirmation; stale actions are
disabled until the Manager signs in again. Group labels never grant access.
The dialog's computer chooser uses enrollment metadata, so enrollment
administrators can organize computers outside their observe scope without
receiving endpoint identity keys or pixels. Authorized
inventory refresh runs independently
for each signed-in deployment and rejects stale responses. Availability is not
authentication or endpoint admission. Profiles can be disconnected or
forgotten locally without removing the server or its data.

Opening an enrolled computer creates one separate view window per deployment
and computer identity; opening it again focuses that window. Signing out,
session expiry, revoking the current Manager device, or closing the console
closes windows owned by that deployment. A refreshed inventory also closes a
window if its computer is no longer enrolled or authorized. These windows are
lifecycle shells: before opening one, the app loads the current account's
bounded `observe` scope from a v2 endpoint and checks the selected computer;
scope refresh, expiry, failure, and session invalidation close existing
windows. This scope response is UI state only. The app does not request
endpoint admission or receive pixels while the native capture/media path is
unavailable.

## Build

Use Qt 6.10.x, CMake 3.28 or newer, Ninja on macOS, Visual Studio 2022 on
Windows, and a Rust toolchain with the requested target installed. Set
`QT6_ROOT` to the platform-specific Qt 6.10 installation directory. CMake
rejects Qt versions outside the initial 6.10 compatibility line. SARibbon is
fetched at the immutable source revision recorded in `CMakeLists.txt`. The
`native-auth` static library uses its lockfile and provides Ed25519 signing to
the Qt app.
The Mac bundle targets macOS 13.0 or newer and uses the approved manager icon
fallback; Windows embeds the provenance-tracked manager ICO in the executable.
The unfinalized development bundle ID is `systems.helicoid.manager.dev`.

```sh
export QT6_ROOT=/path/to/Qt/6.10.0/macos
cmake --list-presets
cmake --preset manager-macos-arm64-release
cmake --build --preset manager-macos-arm64-release
```

Use `manager-macos-x86_64-release` for an Intel build and
`manager-windows-x64-release` from a Visual Studio 2022 developer environment
for Windows x64. On Windows, set `QT6_ROOT` to the matching MSVC Qt 6.10.x
directory in the developer shell. Each preset builds into its own ignored
`.build/` directory. The CMake `package` target runs Qt's platform deployment
tool during installation, includes the pinned SARibbon license, and packages
the app as a macOS DMG or Windows ZIP:

```sh
cmake --build --preset manager-macos-arm64-release --target package
cmake --build --preset manager-macos-x86_64-release --target package
cmake --build --preset manager-windows-x64-release --config Release --target PACKAGE
```

Packages are written into the preset's build directory. These are unsigned
development packages; release signing and notarization are separate acceptance
gates. The configured Qt kit's applicable license and third-party notices still
need to be assembled into a production release package.

For a macOS package with the native transport, configure with
`HELICOID_MANAGER_WEBRTC_DYLIB` set to an absolute path for the architecture-matching
`libhs_webrtc_shared.dylib`. Set `HELICOID_MANAGER_REQUIRE_WEBRTC=1` to make
configuration fail when that library is missing. The install step places it
in `HelicoidManager.app/Contents/Frameworks`, where the runtime loader looks.
The shared CI workflow is configured to build this library from the pinned
`modules/core` submodule and check that the DMG contains the same SHA-256 bytes.

The root `Shared Qt Manager` workflow is configured to build and retain
unsigned artifacts for native macOS arm64, macOS x86_64, and Windows x64
runners. Each macOS job is configured to build the pinned WebRTC dylib for its
architecture and run the Swift wrapper check. Packaging requires the dylib;
the job verifies its hash and architecture in the finished DMG, then runs the
Manager's fixed-bundle loader against the packaged library for engine
initialization and shutdown. The workflow uses Qt 6.10.0 and the presets above.
The macOS package path verifies all four selected icon sources against the
preserved brand manifest, compiles the layered Manager icon, and checks its
same-artwork `.icns` fallback; CI requires the layered result. These unsigned
bundles do not provide signing, notarization, installed desktop acceptance, or
production release proof.

The dependency-free registry target checks deployment-scoped duplicate
computer IDs, independent server availability/session generations, stale
callback and trust-verification rejection, bearer-token clearing, and required
manager-device identity. The same Qt-free harness pins the preview card and grid
dimensions used by the persisted Compact, Standard, Large, and Extra Large
preferences, and checks the standard fallback. Configure and run it independently
of Qt with:

```sh
cmake -S . -B build-manager-pure -DHELICOID_MANAGER_REGISTRY_TESTS_ONLY=ON
cmake --build build-manager-pure --target manager_registry_tests manager_preview_metrics_tests manager_webrtc_runtime_tests
ctest --test-dir build-manager-pure --output-on-failure
```

Saved profiles contain a local profile ID, display name, and server URL. The
verified deployment ID, grant-signing public key, and authority generation are
stored separately in local application settings and never included in profile exports; imported
profiles must be verified again. The server administrator can read the
authority locally with
`curl --fail http://127.0.0.1:8787/v2/deployment/authority`; compare its
`authority_generation` and calculate the key fingerprint with
`curl --fail http://127.0.0.1:8787/v2/deployment/authority | jq -r '.grant_signing_public_key' | xxd -r -p | openssl dgst -sha256 | awk '{print $NF}'`.
Compare the deployment ID, key fingerprint, and `authority_generation` through
an independent trusted organization channel. The Manager rejects a lower
generation for the same deployment and a changed key at the same generation.
Sign-in, bootstrap, account MFA confirmation, and MFA recovery compare the
full tuple with the saved local pin before sending
credentials or recovery material. Restore rotates the deployment ID and
authority key, so a detected change marks the profile as mismatched, blocks
sign-in and protected actions, and clears that profile's old local session and
inventory until deliberate re-verification. The old bearer token is not sent to a server with a
changed pin during identity verification or sign-out; the old server-side
session may remain until it expires. Passwords,
bearer tokens, and recovery material are not persisted.
The Ed25519 seed is stored in macOS Keychain or Windows Credential Manager and
is kept in process memory only while the challenge is being completed. HTTPS
profiles use normal platform certificate validation; HTTP is allowed only for
loopback development servers. Requests reject redirects.
Closing the Manager clears local session and inventory state immediately,
aborts in-flight requests, then checks each saved deployment ID, authority
key, and generation before attempting server logout. The window closes after those
bounded requests finish. If the server cannot be reached or either pin no
longer matches, the bearer token is discarded without transmission and the
server session may remain valid until its normal expiry.

File > Export Profiles & Preferences writes a bounded, versioned
`helicoid.manager.migration` schema-v2 JSON bundle with connection profiles and
deployment/account-scoped monitoring inclusion, Grid/List, and preview-size
preferences. File > Import accepts that bundle
and the earlier schema-v1 connection-profile document. Import previews the
new profiles, preference scope count, computer view, and any scoped preview
sizes before writing;
matching scoped preferences are replaced only after confirmation. Earlier
schema-v2 bundles without `computer_view` remain accepted. A bundle produced
by the earlier global preview-size format is also accepted. The opaque scope
hashes and computer IDs are validated and remain preference data, not server
authorization.
New profiles carry no deployment trust and need identity verification before
sign-in; a matching local profile keeps its existing local trust pin.
The bundle excludes deployment trust pins, sessions, passwords, tokens,
recovery material, and signing keys. The setup draft stays local because host
addresses and filesystem paths may not apply on another computer. The existing
Managers do not persist reusable password or bearer-token credentials. On
macOS, the Qt adapter uses the same protected Keychain item as the previous
Swift Manager, so the same Mac reuses that device identity without exporting
the key; it also upgrades the earlier Qt adapter's raw 32-byte item to the
versioned Keychain format. On Windows, the Qt adapter uses Windows Credential
Manager; the inspected predecessor has no matching persisted signing-key
adapter, so a new Qt identity may need normal approval on each server. Moving a
bundle between machines or operating systems never moves the private key.

The v2 bundle shape is fixed and rejects unknown fields:

```json
{
  "format": "helicoid.manager.migration",
  "schema_version": 2,
  "profiles": [
    {
      "profile_id": "00000000-0000-4000-8000-000000000001",
      "display_name": "Main office",
      "endpoint": "https://server.example.org"
    }
  ],
  "preferences": {
    "monitoring_inclusion": {
      "9e90baa7eb649a09f07ab6a31ad0ef9c4fedf1fd0fdb62df088ae4c275b781b9": {
        "all_computers": false,
        "computer_ids": ["server-computer-id"],
        "panel_visible": true,
        "computer_view": "grid",
        "preview_size": "standard"
      }
    }
  }
}
```

The sample scope digest is illustrative. A real digest is calculated from the
length-prefixed authenticated deployment ID and account ID. New exports carry
scoped `computer_view` (`grid` or `list`) and `preview_size` (`compact`,
`standard`, `large`, or `extra_large`) preferences. Importers accept earlier
schema-v2 bundles without these optional values, as well as the top-level
global `computer_view` and `computer_preview_size` fields from earlier formats.

The Manager can register its protected device key as a pending manager on one
explicitly selected server, and an enrollment administrator can approve or
revoke a selected device. The server verifies key possession and rechecks
session authority before writing. Administrators can create accounts with an
initial capability set; new accounts remain inactive until the user confirms
TOTP and independently stores recovery codes. The one-time enrollment material
is shown in a temporary dialog and is not saved by Manager. The Server ribbon
can initialize the first administrator with a deployment-bound device proof,
then separately confirm MFA after recovery storage. The Accounts ribbon can
also grant or replace an active account's computer capability for all
computers or selected loaded computers; this requires a current approved
Manager session and recent MFA. Account capability edits use the selected
account's owning server and preserve at least one active security
administrator; only a security administrator can change that role. Operator
accounts can enroll their own optional MFA before using these fresh-MFA
administrative actions. Security administrators can set the operator MFA
policy through a scoped dialog that displays the effect of requiring MFA and
confirms before applying it. Active accounts can be disabled or re-enabled;
disabling revokes their current sessions, and the last active security
administrator is protected. Accounts retained from a restored backup are
marked recovery-quarantined in the account table and cannot be re-enabled.
Restore recovery review and release are available through the server's local
`helicoid-server recovery` command and the TUI Backup and Restore page; release
leaves restored accounts and endpoints quarantined. This is deliberately a
local server recovery workflow and is not exposed in the remote Manager UI.

`File > Export Authorized Inventory (CSV)` exports the records most recently
loaded under currently signed-in server sessions. It includes only those
scoped records, caps output at 10,000 computers and 8 MiB, escapes CSV cells,
and prefixes spreadsheet formula-like values. It contains no session,
device-key, or credential material; it does not refresh the server first.

The Files workspace contains a bounded manager-to-computer job and upload
path. The server exposes the recipient-bound v2 routes over TLS and loopback,
and `/health` reports `features.file_jobs=true`; legacy v1 file-job routes
remain unavailable. The server and Computer source clients sign a macOS UID
or Windows SID plus the OS account label, bind each new target to the verified
local user, and pause delivery when that identity changes. Before
confirmation, the Manager fetches a fresh recipient session and names the
account, computer, server, and private Received Files destination for each
selected computer. Recipient preflights run one at a time; a server-local job
can include up to 50 computers. The Manager submits every exact confirmed
session ID, and the server rejects the job if any recipient changed. The
reviewed server origin stays pinned through the upload. Job history shows each
recipient label alongside Waiting for Computer, Waiting for Recipient, or
Recipient Unavailable, and shows the job expiry time in local time. The server
expires unfinished uploads or delivery after its 24-hour deadline and releases
their stored payload. Job cancellation names the owning server and rechecks the
deployment-scoped row, current session, and cancellable state after exact-ID
confirmation. With All servers selected, the Manager checks delivery
availability on each signed-in deployment that has enrolled computers, lets
the operator select combined recipients, and creates one separately submitted
server-local job for each selected deployment. An unavailable deployment or
recipient is excluded with its identity shown while eligible deployments can
continue. These jobs can succeed or fail independently; there is no
cross-server transaction. The combined selection is bounded to 50 computers
total, and recipient checks run sequentially across servers. These source
changes still need a Qt build, Windows compilation, and V48 runtime evidence
before they form an accepted delivery workflow. The Files workspace
also has source UI for saving a request draft, publishing it, closing an open
request with exact-ID confirmation, and combining request rows across signed-in
servers by default. A per-server filter is available, and each refresh or
mutation stays bound to the owning server. The request list refreshes when its
tab or server filter is selected and when a new server session signs in while
the list is open. The owner-scoped request routes use authenticated v2 APIs
over TLS as well as on the loopback development router. Job and request content
share bounded deployment storage accounting. The
Manager also has source UI for reviewing Received submissions and saving one
selected file through `QSaveFile`, with bounded chunks and a final size and
SHA-256 check. It rejects path-like and reserved filenames, fences the chosen
server origin and session generation through the save dialog, and publishes
only after verification. A separate action removes retained content for a
received submission after typing its exact ID and confirming the logical
deletion; request and file metadata stay available, while SQLite free pages
and prior backups may still retain the bytes. Windows and macOS Computer both
have source for the endpoint request picker and chunked, receipt-verified
upload. Their platform builds and live
endpoint verification remain pending. The Files workspace also queries the
authenticated `/v2/files/storage` summary separately for each signed-in server.
It shows logical payload reservations, available quota, active jobs/uploads,
and retained Received files. It does not report database pages, backups, or
filesystem free space; quota administration and scheduled retention remain
unavailable. The Files workspace combines request rows
from signed-in servers by default and offers a per-server filter; refresh and
request rows carry a deployment-scoped ID, and selection/mutations retain both
the deployment and owning profile. The Linux TUI now has a
separate loopback API download path with explicit selection, bounded chunk
streaming, SHA-256 verification, and no-overwrite publication; the authenticated
v2 TLS routes expose the same owner-scoped request workflow. The Qt source
still needs a build and runtime verification. The Manager now requests and
verifies short-lived observation grants, exchanges certificate-attested
offer/answer and ICE signals, proves the current native channel binding, and
has a receive-only frame display path. After a first frame, it requests a
fresh grant when the current one expires. Windows Computer source verifies and
redeems the grant, captures the eligible physical console, and sends frames
through its native peer. These paths remain fail-closed because the native
observation capability bits are disabled. The Windows integration workflow is
configured to build and package the pinned DLL, and the Mac Manager workflow is
configured to package its matching dylib; neither workflow has run against the
current source changes. No installed preview is established.

An access assignment can be revoked from the combined Accounts table. Manager
requires an exact phrase containing the assignment ID and current revision;
the owning server rechecks that revision, session authority, and recent MFA in
the deletion transaction.

## Current boundary

| Capability | State |
| --- | --- |
| Persistent console and Office 2021 SARibbon shell | Source implemented; Qt build not available in the current workspace |
| Connection profile add, schema-v1 import, schema-v2 profile/preference migration export/import, and local forget | Source implemented; import is explicit, bounded, confirmation-gated, includes the portable Grid/List choice, excludes credentials and trust pins, and does not remove server resources or data |
| Deployment authority verification | Source implements per-profile deployment-ID, Ed25519 key, and monotonic generation pins, out-of-band comparison, rollback/same-generation key-change rejection, mismatch blocking, and untrusted imports; Qt build and installed workflow verification remain pending |
| Sign-out and normal close | Local authority and inventory are cleared immediately; remote logout is sent only after the saved deployment ID, authority key, and generation are rechecked, and close waits for bounded in-flight cleanup |
| Server availability and selected-server details | Implemented with bounded requests and redirect rejection. The Server ribbon's Network & Certificates command runs the selected-server health check. Successful HTTPS responses show the negotiated TLS protocol and immediate peer-certificate SHA-256, separately from deployment identity verification. The timestamped public `/health` report includes status, storage schema, bootstrap state, and reported capabilities; it does not verify deployment identity or installation state |
| Authority drain and restore cutover review | Server workspace source reads the v2 cutover and recovery-quarantine summaries and provides fresh-MFA drain, cancel, replacement-approval, and quarantine-release actions. Writes use exact identity phrases, fixed fencing methods, count snapshots, and explicit recovery acknowledgments. Approval remains an operator attestation; this UI does not verify old-host shutdown or report peer adoption. Qt 6.10 is unavailable in the current build environment, so the desktop target remains unbuilt |
| Endpoint recovery re-enrollment | Windows and macOS Computer source provides an explicit local forget-and-re-enroll path, retains received files, and creates a new local key for a fresh invitation. Version-4 endpoint records pin deployment ID, grant-signing key, and authority generation; protected operations stop when the live checkpoint differs. The old server-side endpoint still requires Manager quarantine; Windows interactive build and acceptance remain unverified |
| Server setup workspace | Saves a local draft with deployment, Linux SSH address, installation mode, separate data/backup paths, and LAN TLS or loopback scope. Check Host performs a read-only SSH preflight with strict existing `known_hosts` verification and bounded output. It checks supported OS/architecture, systemd, noninteractive sudo, storage, the selected TCP port, existing installs, and whether the configured LAN bind address is assigned; Compose mode also checks Docker Engine reachability and Compose 2.17.0 or newer. For an installed root-owned backend it also reads lifecycle capabilities and operation status, including precise unavailable reasons for updates/rollback and deployment restore. It does not change the host or inspect TLS files. SSH install/apply, Windows/macOS local hosting, Qt build, and installed workflow verification remain unavailable |
| Device-key challenge, MFA, and per-deployment account session | Source implemented through v2; signing seed uses Keychain/Credential Manager; package build still needs Qt and platform proof |
| Deployment-scoped manager registry | Composite computer and file-request IDs, independent per-deployment sessions, and profile-incarnation callback fencing are source implemented; standalone C++ boundary tests pass for the previously covered registry behavior |
| Account, manager-device and access-assignment reads | Implemented through permission-checked v2 endpoints available on the TLS network router; IDs are deployment-scoped and refresh-generation fenced |
| Manager key registration, approval, and revocation | Implemented through v2 registration and fresh-MFA approval/revocation routes; revocation confirms the exact Manager ID and ends that device's active sessions; needs Qt build and installed server/client verification |
| First administrator bootstrap | Source implemented through the deployment-bound v2 proof; one-time MFA material is shown without persistence, then separately confirmed |
| Grid/List, search, server, group and enrollment-state filters | Group options are scoped by deployment and come only from authorized inventory records; Accounts also has a server filter |
| Computer group administration | Source implements per-deployment list, create, rename, membership, and exact-ID delete over v2; membership remains metadata and the Qt build is pending |
| Endpoint invitation and approval | Source implements one-use codes, proof-backed registration, invitation cancellation, fresh-MFA approval, and epoch-fenced revocation; Qt build and live endpoint client verification remain pending |
| Authorized inventory refresh | Source implemented across signed-in deployments with per-deployment response generations; Qt build and installed verification remain pending |
| Authorized inventory CSV export | Source exports only loaded records with a current local session, bounds output to 10,000 records/8 MiB, writes atomically, and escapes spreadsheet formula-like values; Qt build remains pending |
| Separate computer view windows | Source checks the account's bounded v2 observe scope, opens one deployment-scoped viewer per computer, expands the preview canvas to the available viewer area, and automatically starts its view-only grant flow. The manual button remains available after a failed attempt; live endpoint admission and pixels remain unavailable while native observation capability is disabled |
| Viewer screenshots | Source saves a full-resolution PNG from a fresh admitted frame to a user-selected local path atomically; save is refused after the short grant expires or the frame becomes stale, and observation teardown clears retained pixels. Qt build, UI execution, and screenshot acceptance remain pending |
| Account provisioning and initial capabilities | Source implemented through a fresh-MFA v2 route; account starts pending MFA and Manager shows the one-time setup material |
| MFA enrollment confirmation | Source implemented through the existing one-use confirmation contract; user attests that recovery material was stored independently |
| Self-service MFA enrollment | Source implemented for active accounts without an active factor; it returns one-time setup material and requires sign-out/sign-in before fresh-MFA writes become available |
| Access assignment create/replace | Source implemented through v2 with an in-transaction permission, session, approved-device, and fresh-MFA check; target and computer scope are server-local |
| Access assignment revocation | Source implemented through v2 with exact Manager confirmation, an expected-revision transaction, and fresh-MFA authorization; Qt build and installed verification remain pending |
| Account capability edits | Source implemented through v2 with a fresh-MFA transaction, security-role ceiling, active-MFA prerequisite for security administrators, and last-security-admin protection |
| Account disable/reactivate | Source implemented through v2 with revision checks, session revocation, and last-security-admin safeguards |
| MFA recovery | Source implemented through the password plus one-use recovery-code v2 flow; recovery revokes existing account sessions and issues no session |
| Operator MFA policy changes | Source implemented through the security-admin, fresh-MFA v2 route; requiring MFA revokes factor-free operator sessions |
| Security audit metadata and retention | Accounts workspace reads bounded cursor pages from one selected deployment and clears the view when that server session ends; retention changes require fresh MFA and confirmation. The server stores metadata-only events transactionally with covered mutations and applies the configured window. Manager Qt build and installed audit acceptance remain unverified |
| Manager approval and access-assignment mutations | Source implemented through server routes; Qt build and installed server/client verification remain pending |
| Manager-to-computer file jobs | TLS and loopback v2 routes are mounted and health advertises support; source includes signed recipient labels, fresh preflight, exact-session creation, and profile-pinned upload. Qt/Windows builds and D29 live delivery remain unverified |
| File-request workflow surfaces | Manager source saves, combines, publishes, closes, reviews, downloads, and logically deletes received submission content with exact-ID confirmation through the owner-scoped v2 request feature; Windows and macOS Computer request pickers and chunked uploads remain source-only |
| Sign-in and grant-bound view-only preview | Source paths implement grant verification, attested signaling, native channel proof, frame display, Windows physical-console capture, and fresh-grant renewal. Opening an authorized computer viewer now starts this view-only flow automatically; async callbacks recheck the trusted deployment/key/generation tuple and stop on authority mismatch. Observation remains fail-closed because native capability bits are disabled; Windows and macOS integration workflows are configured to build, package, and hash-check the pinned native libraries, but the current source changes have not run on those CI runners and installed preview is unverified |
| Remote input and explicit control | Unavailable |
| Full server administration from the Manager | Unavailable; the Linux TUI is a separate partial surface |

The source-only Qt shell is not evidence of a packaged or accepted Windows/macOS
Manager. The release targets, native secure-store adapters, installed desktop
behavior, viewer lifecycle, and control authorization still require their own
build and runtime evidence.
