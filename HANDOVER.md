# Handover: Veyon for macOS

Written on 2026-11-15 for a Claude Code session running **on the MacBook**. Everything so far was developed on a
Windows PC that cannot build or run the macOS version, so nothing in this repository has been seen on a real Mac
yet. Your first job is to change that: build it, install it, and check that it behaves as described below.

Read this file fully before doing anything. It is a snapshot; where it disagrees with the repository or GitHub,
trust those and say so.

## What this project is

A macOS port of [Veyon](https://veyon.io) (classroom management: a teacher's *Veyon Master* monitors and controls
student computers running the *Veyon Service*), plus an add-on store with bundled add-ons. Qt 6 / C++20 / CMake,
GPL-2.0.

- Repository: `0mattsmith/VeyonMacOS` (public). Upstream: `veyon/veyon`.
- Default branch: **`macos-addons`**.
- Owner: Matt, a teacher/IT admin. He is not a C++ developer; explain results in plain language and show him what
  changed rather than how.

## How we work

- **Branches and PRs.** Every change goes on its own descriptively named branch (e.g. `macos-install-config`) with
  a PR against `macos-addons`. Never commit to `macos-addons` directly.
- **Ask before merging.** Matt decides when a PR is merged. Merge with a merge commit (`gh pr merge --merge`), not
  squash, so PRs stacked on each other stay clean.
- **Auto-fix.** Matt wants Auto-fix turned on for every PR (the Claude desktop app then wakes the session on CI
  failures, conflicts and review comments).
- **CI is the compile check for other platforms.** `.github/workflows/` builds Linux (`build.yml`), Windows
  (`windows.yml`) and macOS on Apple silicon and Intel (`macos.yml`) for every push. The repo is public, so minutes
  are free; only 5 macOS jobs run at once. A newer push to a branch cancels the older run, so cancelled runs right
  after a push are normal.
- **`gh` targets the wrong repo if an `upstream` remote exists.** Run `gh repo set-default 0mattsmith/VeyonMacOS`
  once in the clone, or pass `-R 0mattsmith/VeyonMacOS`.
- **Code style.** Veyon's own: tabs, spaces inside parentheses, `QStringLiteral`, `tr()` for every UI string.
  macOS-only code goes behind `#ifdef Q_OS_MACOS` or into `plugins/platform/macos/`. The Cocoa helpers use the
  Objective-C runtime from plain C++ (`MacOsCocoaHelpers.cpp`); keep it that way unless there is a reason not to.
- **Line endings.** Commits are LF. (The Windows clone used `core.autocrlf=true`; on the Mac nothing special is
  needed.)

## Building on the Mac

These are the steps `.github/workflows/macos.yml` runs; that file is the reference if anything here is out of date.

```bash
git clone --recursive https://github.com/0mattsmith/VeyonMacOS.git
cd VeyonMacOS
brew install ninja qt qca openssl@3 lzo jpeg-turbo libpng libvncserver openldap

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix qca);$(brew --prefix openssl@3);$(brew --prefix openldap)" \
  -DWITH_LTO=OFF \
  -DVEYON_MACOS_SIGNING_IDENTITY=-

cmake --build build --parallel
cmake --build build --target macos-package
```

- `-DVEYON_MACOS_SIGNING_IDENTITY=-` signs ad hoc. That is fine for testing, but macOS ties privacy permissions
  (Screen Recording, Accessibility, Full Disk Access) to the signature, so **every ad-hoc rebuild loses them** and
  they must be granted again (to `Contents/Helpers/veyon-server` itself, see `plugins/vncserver/macos-builtin/README.md`).
- **CI signs with a self-signed certificate** since 2026-11-27: "Veyon Code Signing (Matt)", SHA-1
  `7189E5689E133B8D9ABF2740179C1BB5D257532C`, valid until 2036, stored in the repository secrets
  `MACOS_CERTIFICATES_P12`, `MACOS_CERTIFICATES_PASSWORD` and `MACOS_SIGNING_IDENTITY` (the SHA-1: `codesign`
  only finds an untrusted identity by its hash). The backup (`.p12` and password) is in Matt's
  `~/Documents/Veyon Signing`. Privacy permissions granted to "Veyon" therefore survive updates from CI. It is not an
  Apple-trusted certificate: no notarization, Gatekeeper still warns when the package is opened from Finder. A
  "Developer ID Application" identity (Apple Developer Program) would replace it through the same secrets. To sign a
  local build the same way, import the `.p12` into a keychain and pass the SHA-1 as signing identity.
- Outputs: `build/macos-staging/Veyon.app` (run in place for quick checks) and `build/macos-dist/*.pkg` and `.dmg`.
- Install: `sudo installer -pkg build/macos-dist/*.pkg -target /`. This installs `/Applications/Veyon.app`,
  `/Applications/Veyon Configurator.app` and the LaunchDaemon `io.veyon.veyon-service`.
- Uninstall: `sudo /Applications/Veyon.app/Contents/Resources/uninstall.sh --purge`.
- Command line tool: `/Applications/Veyon.app/Contents/Helpers/veyon-cli` (`about`, `feature list`,
  `config get <Group/Key>`, `service status`, `authkeys ...`).
- To build an open PR instead of `macos-addons`: `gh pr checkout <number>`.

Installing and granting permissions changes Matt's Mac. Tell him before you install, uninstall, or change system
settings, and let him type passwords and approve permission prompts himself.

## How Veyon is laid out on macOS

- **`Veyon.app`**: `Contents/MacOS/veyon-master` is the only main program. Everything else (`veyon-server`,
  `veyon-worker`, `veyon-service`, `veyon-configurator`, `veyon-cli`) is in `Contents/Helpers`, so that macOS does
  not treat them as instances of Veyon.app.
- **`Veyon Configurator.app`**: a tiny launcher (`macos/ConfiguratorLauncher.c`) that starts
  `Veyon.app/Contents/Helpers/veyon-configurator`.
- **Service**: LaunchDaemon `io.veyon.veyon-service` (root). It starts `veyon-server` inside the logged-in user's
  session with `launchctl asuser` (still as root). The server starts `veyon-worker` processes as the user for
  individual features (screen lock, demo, messages, the menu bar icon).
- **Configurator runs as root**: it restarts itself through
  `osascript -e 'do shell script … with administrator privileges'` (`MacOsCoreFunctions::runProgramAsAdmin`).
- **Screen capture** uses Veyon's own builtin VNC server `plugins/vncserver/macos-builtin` (ScreenCaptureKit for
  the picture, `CGEventPost` for input), which needs the Screen Recording and Accessibility permissions. The older
  `plugins/vncserver/macos-screensharing` plugin (Apple's Screen Sharing) is still there but no longer the default:
  on macOS 26 it shows the login window instead of the user's session. See `macos-builtin/README.md`.
- **Paths**: system-wide data in `/Library/Application Support/Veyon` (keys, add-ons, config templates); per-user
  data in `~/Library/Application Support/Veyon`; settings in `/Library/Preferences` (QSettings, system scope).

## What has been merged, and what to verify on the Mac

All of this compiled and passed CI, but CI cannot see a Dock, a menu bar, or a clipboard. Each row is a claim to
check. Record what you find; a failed check is the most useful thing you can report.

| PR | Change | How to check on the Mac |
|---|---|---|
| #1 | Logs default to `%LOGS%`: `~/Library/Logs/Veyon` for user programs, `/Library/Logs/Veyon` for the root service. Screenshots default to `~/Pictures/Veyon`. Backslashes in configured paths (`%HOME%\Desktop`) are converted. Configurator warns when it is not running as root. | Start Master and the service, then look for `Veyon*.log` in both log folders (also visible in Console.app). Take a screenshot in Master and find it in `~/Pictures/Veyon`. Before this PR macOS probably wrote **no** log files at all, because `/tmp` and `/var` are symlinks and the logger rejects symlinked paths. |
| #2 | `veyon-server` and `veyon-worker` no longer appear in the Dock. The worker's tray icon is a menu bar item: a monochrome eye-in-a-circle glyph with a menu showing the service status, "Open Veyon Master" and "Open Veyon Configurator". | With the service running and a user logged in: no Veyon helper icons in the Dock, even after using Lock/Demo/Message from a Master. A Veyon icon in the menu bar; both menu entries open the right app, and an already running Master comes to the front instead of starting twice. The glyph is drawn in code (`SystemTrayIcon::menuBarIcon()`) and has never been seen; judge its proportions. "Hide tray icon" on the Configurator's Service page should hide it. |
| #4 | The installer imports `/Library/Application Support/Veyon/Config/install.json` if present, then removes it (the macOS answer to Windows' `/ApplyConfig=`). | Covered by CI's installer test; only re-check if you touch the installer. Documented under "Veyon on macOS" in `README.md`. |
| #5, #10 | LDAP plugin builds on macOS with Homebrew's OpenLDAP bundled into the app. TLS certificates are verified against macOS's CA file (`/etc/ssl/cert.pem`). | Configurator shows an LDAP page. Real directory testing needs a school LDAP/AD server; ask Matt whether he has one. |
| #6 | Tooltips on directory fields list the placeholders (`%HOME%`, `%DOCUMENTS%`, `%LOGS%` …). `*.webloc;*.fileloc` are excluded when collecting files. "Log in" and "Install updates and power down" report that they are not supported on macOS clients instead of silently doing nothing. | Hover the directory fields in the Configurator. From a Master, try "Log in" against a Mac client and check that a clear message appears. |
| #7 | The root Configurator works on behalf of the user who started it: file dialogs open in that user's folders, `%HOME%`/`%DOCUMENTS%` etc. expand to that user's folders, folders picked there are stored as placeholders, and exported keys/settings/add-ons are chowned to the user. | In the Configurator: export an authentication key and check with `ls -l` that you own it and can read it. Use a "Browse" button; it should open in your home folder, not `/var/root`. Pick a folder under your home and check the field shows `%HOME%/...`. |
| #8 | The elevated Configurator is started with `launchctl asuser <uid>` so it runs inside the user's session. This is meant to fix **copy and paste not working** in the Configurator. The admin prompt now carries an explanation ("Veyon Configurator needs administrator privileges to …"). | **The cause was inferred, not confirmed.** Copy text elsewhere and paste into a Configurator field, and the reverse. If it still fails, see "Open questions". Also read the admin prompt: the header will probably still say "osascript". |
| #9 | Access control compares user group names case-insensitively ("Teachers" on Windows matches "teachers" on macOS). Regular expressions and location names are unchanged. | Only testable with access control rules and two machines. |
| #11 | The original Veyon icons were redrawn as rounded colour tiles to match the add-on icons. They are generated by `addons/tools/make-icons.py`. | Look at Master's toolbar and the Configurator's page list for icons that read badly at real size or in dark mode. |
| #3 | CI: no duplicate `pull_request` runs for same-repo branches; superseded runs are cancelled. | Nothing to check. |

## Results of the first test on the Mac (2026-11-19)

Tested on Matt's MacBook Pro (Intel, macOS 26.7.2) with CI's `veyon-macos-X64` package of `macos-addons`. Homebrew
has no Intel bottles for `qt`, `qca` and `openldap` on macOS 26, so a local build was skipped; download the CI
artifact (`gh run download <run> -n veyon-macos-X64`) to test a branch.

- **Works:** installing over an older version, log files (#1), server/worker not in the Dock (#2), copy and paste in
  the Configurator (#8), creating keys, screenshots, remote control of a Windows PC from the Mac's Master, Browse
  buttons (slow to open).
- **Open question 1 answered:** saving keys failed because Matt's configuration had paths starting with `~`
  (`~/Library/Containers/Veyon/...`), which `Filesystem::expandPath()` does not expand. The Configurator then tried
  to create `/~/...`. The same broke personal settings and screenshots. Matt's settings were reset to the defaults;
  Veyon should expand `~` or reject it (not done yet).
- **Open question 2:** new keys belong to group `admin`, not `wheel`, so admin accounts can read them; standard
  accounts still need "Set access group".
- **Open question 3:** copy and paste works, so the clipboard check was not needed.
- **Screen capture:** every connection through Screen Sharing showed the login window (see "How Veyon is laid out").
- **Smaller issues:** Master saves an empty 0-byte screenshot when not connected; every log repeats
  `Configuration: cannot replace sub data map with a value!` (cause unknown); the Configurator asks for the admin
  password on every start and the prompt is labelled "osascript".

## Open pull requests

Check their current state with `gh pr list` and `gh pr checks <n>`; both were green and conflict-free when this was
written.

- **#12 `macos-toolbar-groups` — grouped Master toolbar.** 24 buttons become about 15 slots: View/Control and
  Distribute/Collect as two stacked half-height buttons in one slot, one combined Website button, a menu-only
  Power button that also holds Log off and Log in, a Scripts menu, and shorter labels ("Message", "Start app",
  "Web filter"). A "Group related features" item in the toolbar's right-click menu restores the old layout.
  `Master/ConfirmUnsafeActions` now defaults to true, so reboot, power down and log off ask first.
  **This is the PR that most needs eyes on a Mac.** Its description has a "Not verified" list: toggle grouping and
  "Show icons only", open each drop-down, run a Demo and toggle grouping while it runs, disable an add-on and
  check its group adapts. Matt was in the middle of checking it by hand.
- **#13 `macos-icon-themes` — selectable icon style.** A new "Icon style" setting on the Configurator's General
  page (next to "Widget style", renamed from "Style", and "Color scheme") with three sets: Modern (the tiles),
  Monochrome, and Classic (the original icons, plus add-on icons drawn to match). The session that wrote it was
  stopped twice by usage limits, so **read the PR description and diff before trusting that it is complete**.
  Check that switching the setting changes every icon in Master, the Configurator and the Add-On Store.

Matt also said he wants to **add more widget styles** after #13 and discuss that first. Don't start it unasked.

## Open questions only a Mac can answer

1. **Does saving authentication keys work?** Matt originally reported that the Configurator "doesn't have the
   necessary Read and Write permissions" and failed to save keys. No code defect was found; the suspicion is that
   the Configurator was not actually root. With it open, run:
   ```bash
   ps -o user=,args= -p $(pgrep -f veyon-configurator)
   ```
   It should say `root`. Then create a key pair on the Authentication keys page. If it fails, the exact error text
   is what matters. Keys live in `/Library/Application Support/Veyon/keys/{private,public}/<name>/key`.
2. **Private key access group.** New keys belong to group `wheel`, so a teacher account cannot read the private
   key until an access group is set on it (Configurator → Authentication keys → Set access group; `staff` = all
   local users, `admin` = administrators). Check whether Master's key authentication fails until that is done,
   and if so whether the Configurator should pick a sensible group by default.
3. **Clipboard (PR #8).** If copy and paste still fails, test the mechanism directly (copy some text first):
   ```bash
   osascript -e 'do shell script "/usr/bin/pbpaste" with administrator privileges'
   osascript -e "do shell script \"/bin/launchctl asuser $(id -u) /usr/bin/pbpaste\" with administrator privileges"
   ```
   If the first prints nothing and the second prints the text, the diagnosis was right and the fix should work.
   If both print the text, the cause is something else (look at the Edit menu / standard key bindings of a Qt app
   running as root).
4. **Privacy permissions (TCC).** Nothing prompts for or checks Screen Recording, Accessibility or Full Disk
   Access yet. Find out what a fresh install actually needs: does remote view/control of this Mac work from a
   Master, does input blocking during screen lock work, can the worker write into `~/Desktop` or `~/Documents`
   (file transfer) without a prompt? A first-run check that names the missing permission and opens the right
   System Settings pane was proposed but not built.
5. **Signing.** Answered: there was no Developer ID and no signing secret, so every CI package was ad-hoc signed and
   lost its privacy permissions on each update. CI now signs with a self-signed certificate (see "Building on the
   Mac"); a Developer ID is still needed for notarization.

## Planned but not started

- **Auto-update for Windows, Linux and macOS.** Agreed design: the root/SYSTEM *service* does the update so no
  password prompt is needed. Each release carries a `latest.json` (version, download URLs, SHA-256) signed with an
  Ed25519 key whose public half is compiled into the app. macOS: verify the `.pkg` signature
  (`pkgutil --check-signature`), then `installer -pkg … -target /`. Windows: run the NSIS installer with `/S`.
  Linux: prefer an APT/RPM repository; in-app only notifies. Configurator options: on/off, notify vs. install,
  stable/beta channel, and "only when no lesson is running" (an update restarts the service). Not using Sparkle,
  because it is built for per-user apps and would bring back password prompts.
- **TCC first-run check** (see open question 4).
- **More widget styles** (see above).

## Things that are easy to get wrong

- The Configurator is root. `QDir::homePath()` and `QStandardPaths` point at `/var/root` there; use
  `VeyonCore::filesystem().userHomePath()` / `userStandardLocation()` for anything shown to or saved for the user,
  and `transferOwnershipToUser()` after writing a file on the user's behalf (`core/src/Filesystem.{h,cpp}`).
- Path placeholders are expanded per process in `Filesystem::expandPath()`. `%LOGS%` and `%APPDATA%` mean
  different folders for root and for a user, on purpose.
- The server and workers must never become Dock applications:
  `VeyonCore::setupApplicationParameters( component )` sets `QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM`
  before the application object exists, and `MacOsPlatformPlugin` switches them to accessory apps and unsets the
  variable again so programs started from there are unaffected.
- `/tmp` and `/var` are symlinks to `/private/...`; code that rejects symlinked paths (the logger does) must not
  be pointed at them.
- Anything that changes shared code must still build on Linux (Qt 5 and Qt 6) and Windows; CI will tell you.
- Renaming a UI string drops its existing translations (about 40 languages). Mention it in the PR when you do.

## First session checklist

1. `gh repo set-default 0mattsmith/VeyonMacOS`, then `gh pr list` and compare with "Open pull requests" above.
2. Build `macos-addons` as described. Report build problems before fixing them; a Homebrew Qt newer than CI's is
   the likeliest cause.
3. With Matt's go-ahead, install the package and work through the verification table and open questions 1 to 3.
4. Report to Matt in plain language: what works, what doesn't, with the exact error text or a screenshot for each
   failure. Then agree with him what to fix first.
5. Once #12 and #13 are merged or you have checked them out, repeat the relevant checks for those.

When this file is out of date, update it in the same PR as the change that made it so.
