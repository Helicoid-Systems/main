# Windows development build check

Use Windows 11 x64. Sign in to GitHub with access to Helicoid-Systems.
Download [the verified app bundle](https://github.com/Helicoid-Systems/windows/actions/runs/38051352427/artifacts/11669493591)
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
Windows CI compiled and published both apps and passed 112 domain/client tests;
the user subsequently launched both WinUI windows and reported that they work.
The full UI test matrix remains unverified. No real capture, control, installed service,
MFA, file jobs or production trust is implemented by this bundle.

For live sign-in download [the Windows development server](https://github.com/Helicoid-Systems/server/actions/runs/38054445029/artifacts/11669829225),
extract it, and double-click `start-development.cmd`, or run
`./start-development.ps1` from an existing PowerShell window. Its included
`windows-tester.md` explains the interactive test password and foreground server.
Keep that password private. The launcher approves only `dev-manager-key` and
creates no screen access. Windows launch arguments are:

Close any Manager window opened by double-clicking its executable first.
Open PowerShell in the extracted app bundle folder, then run:

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
