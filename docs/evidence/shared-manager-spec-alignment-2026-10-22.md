# Shared Manager specification alignment — 2026-10-22

The active product, UI, workflow, remote-desktop, and verification documents
now describe the C++20/Qt Widgets/SARibbon Manager on Windows and macOS. SwiftUI
and WinUI 3 remain assigned to the platform-specific Computer applications,
workers, and OS integration. The setup route matrix now describes the
platform-targeted builds of the shared Manager. Historical research and
implementation records remain unchanged.

`python3 scripts/verify-ui-coverage.py` passed: all 74 screen families and 148
platform rows remain present. The validator explicitly does not establish
native UI behavior. The Qt Manager remains unbuilt in this environment, so
these edits provide specification alignment only, not implementation or
runtime acceptance.
