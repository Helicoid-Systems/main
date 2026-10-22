# Manager brand resources

`source/` is a byte-preserving copy of the root user-supplied brand assets and
manifest. The selected four-file `HS Manager App icon.icon` source is retained
with its exact layered composition and provenance.

The macOS Manager target compiles the selected layered
`HS Manager App icon.icon` source into `Assets.car` and a deployment-compatible
`Manager.icns` during CPack installation. All four selected source files are
checked against the preserved brand manifest. If the installed Apple toolchain
cannot compile the layered source, local development packages may use the
checked-in same-artwork `Manager.icns` only after its `PROVENANCE.json` hash
matches. The Mac CI jobs require the layered output and fail if it is missing.

The Windows executable and Qt window icon use the current manager ICO export,
copied unchanged with its original source images, conversion script, and
`PROVENANCE.json`. SHA-256:
`2f0626a56fb18ad0f3321cd5655c06f1586e57b034a947582275e3708489af39`.

These resource assignments preserve source identity. The CMake package step
does not raise the app's macOS 13 deployment target; native Finder/Dock/About,
appearance, scaling, and older-OS fallback behavior still require packaged
runtime inspection.
