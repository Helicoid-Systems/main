# Windows development build check

Use Windows 11 x64. Sign in to GitHub with access to Helicoid-Systems.
Download [the verified app bundle](https://github.com/Helicoid-Systems/windows/actions/runs/38059359725/artifacts/11671909854)
and extract the complete archive. Keep each executable beside all its files.

1. Open `manager/Helicoid.Manager.exe`. Confirm the window opens, navigation
   works and default production connection explains that it is unavailable.
2. Open Computers, enable the explicitly labelled development sample data,
   then try search, Grid/List, inclusion checkboxes and action selection.
   Samples are for UI checks and cannot prove actual computers or media.
3. Open a computer's view. Verify viewing/control remain unavailable with a
   clear reason. Server Setup must not report an installed/verified deployment.
4. Open `computer/Helicoid.Computer.exe`. Confirm its status window opens and
   reports its actual unconfigured/unavailable state.
5. Close and reopen both apps. Report any crash, missing dependency, blank
   screen, unreadable layout or inconsistent status, including the exact message
   and the Windows version. Screenshots are useful for visual problems.

The app bundle is unsigned development output, not a production installer.
Windows CI compiled and published both apps and passed 210 domain/client,
console-policy and decoder tests;
the user subsequently launched both WinUI windows and reported that they work.
The full UI test matrix remains unverified. No real capture, control, installed service,
MFA, file jobs or production trust is implemented by this bundle.

For live sign-in download [the Windows development server](https://github.com/Helicoid-Systems/server/actions/runs/38054445029/artifacts/11669829225),
extract it, and double-click `start-development.cmd`, or run
`./start-development.ps1` from an existing PowerShell window. Its included
`windows-tester.md` explains the interactive test password and foreground server.
Keep that password private. The launcher approves only `dev-manager-key` and
creates no screen access. Windows launch arguments are:

Close any existing Manager window, then double-click
`manager/start-development-manager.cmd`. The bundle includes this launcher
beside the executable and a short tester README. Native CI verified its exact
argument forwarding from a folder with spaces and a different working folder,
plus retained missing-file diagnostics. A PowerShell alternative is:

```powershell
.\manager\Helicoid.Manager.exe --development-server=http://127.0.0.1:8787 --development-manager-identity dev-manager-key
```

The address must be literal loopback. Its manager identity is a development
fixture string. Successful administrator sign-in starts with empty scoped
inventory; administration does not imply screen access.

The new Manager window should show **Development server mode** on Connection
and enabled sign-in fields. Enter `admin` and the test password you chose for
the running server. Greyed-out fields mean this window is in default closed
mode; launching the executable without the switches does not enable sign-in.

## Console metadata diagnostic

The original diagnostic passed the user's unlocked checks but returned eligible
in the reported Win+L check. Lock refusal FAILED for that candidate; native
CI desktop-query correctness did not prove the locked-session matrix. The
repair adds an independent WTS session-lock check. The reported locked-PC
retest passes below; post-unlock recovery and the remaining matrix are pending.
This tool enables no remote access and captures no pixels.

Native Windows CI and the downloaded bundle verified the exact embedded role
icon resources and published sibling ICOs. Both SVG sources remain byte-identical
on Windows checkouts. Role icons and the lock repair are tracked in
[Windows draft PR 1](https://github.com/Helicoid-Systems/windows/pull/1).
The user confirmed both apps open and the supplied blue Manager/dark Computer
icons work in the requested Explorer/taskbar check on the earlier icon bundle
[38058328331](https://github.com/Helicoid-Systems/windows/actions/runs/38058328331/artifacts/11672650381).

The repair passes 210 portable tests, including locked/unknown/query-failure/
transition and malformed native-buffer cases. Schema 2 adds `lockState` and
`lockQuerySucceeded`; `--watch` starts observations immediately in the existing
process and groups repeated results over 30 seconds. Windows SDK layout was
verified by compiling a C++ fixture against the installed headers. The actual
repaired lock result passes the user-reported check below.
RDP is supported but not configured on the tester's PC, so RDP runtime testing is pending.

[Native CI 38059359725](https://github.com/Helicoid-Systems/windows/actions/runs/38059359725)
passed the actual extended WTS query (Unlocked classification) and all 31
sampling observations. This proves query/sampler operation; it does not prove
lock refusal on the tester's PC. The user subsequently supplied all 31 samples:
second 0 was Unlocked/Eligible, second 1 was Locked/DesktopQueryFailed, and
seconds 2–30 were Locked/SessionLocked despite a stable Default desktop.
Every locked sample refused eligibility. Post-unlock recovery remains pending.

To reproduce the metadata check, download [the repaired metadata diagnostic](https://github.com/Helicoid-Systems/windows/actions/runs/38059359725/artifacts/11672762156)
into a fresh folder. Run the first command, immediately press Win+L, wait 40
seconds, then unlock and run the second command. Paste only the resulting JSON.

```powershell
.\Helicoid.ConsoleProbe.exe --watch > .\fresh-lock-check.json
Get-Content .\fresh-lock-check.json
```

Expected: schemaVersion 2 and an ineligible group while locked. If all groups
remain eligible, the repaired lock matrix has not passed.

For the recovery check, run `.\Helicoid.ConsoleProbe.exe` after unlocking.
Expect schemaVersion 2, Unlocked, successful lock query and eligible on the same ordinary physical console. This checks metadata
recovery only; fresh authorization and desktop-generation enforcement are separate.

Check the result contains schemaVersion 2 and the lock fields. An older copy
without these fields cannot verify recovery of the repaired diagnostic.
