# Builtin VNC server (macOS)

Veyon's default VNC server on macOS. It runs inside `veyon-server` in the session of the logged-in user, captures
the main display with ScreenCaptureKit and injects mouse and keyboard input with `CGEventPost`, like the builtin
servers on Windows (UltraVNC) and Linux (x11vnc, PipeWire). It only listens on localhost; Veyon Master reaches it
through `veyon-server`, which does the authentication.

## Why not Apple's Screen Sharing

The older `macos-screensharing` plugin turns on Screen Sharing with "VNC viewers may control screen with password"
and connects with that password only. Tested on macOS Tahoe 26.7 (2026-11-19): such a connection is shown the
**login window**, not the session of the user sitting at the Mac. Logging in to Screen Sharing with a macOS account
("Mac authentication", RFB security type 30) shows the live session, but only for that user's own account; another
account gets a separate, empty session. Veyon cannot know the users' passwords, so Screen Sharing is not usable for
monitoring. That plugin is still available but no longer the default.

## Permissions

| Permission | Needed for | Where |
|---|---|---|
| Screen & System Audio Recording | any picture at all | System Settings > Privacy & Security > Screen & System Audio Recording |
| Accessibility | remote control (mouse and keyboard) | System Settings > Privacy & Security > Accessibility |

macOS ties both permissions to the code signature:

- **Signed with a certificate** (CI uses the `MACOS_CERTIFICATES_P12` secret, see HANDOVER.md): `deploy-bundle.sh`
  signs the helpers with the identifier of Veyon.app, so the single "Veyon" entry in both lists covers
  `veyon-server`, and the permissions survive updates signed with the same certificate.
- **Ad-hoc signed** (local builds with `-DVEYON_MACOS_SIGNING_IDENTITY=-`): the designated requirement is the cdhash of
  each file, so every build is a new program for macOS, and the "Veyon" entry does not cover the helpers. Drag
  `/Applications/Veyon.app/Contents/Helpers/veyon-server` from Finder into both lists after every install.

The server asks for missing permissions once per installed build (it remembers the build in
`/Library/Application Support/Veyon/PermissionPromptState`) and otherwise only logs a warning. Without Screen
Recording permission it keeps retrying every 5 seconds; macOS may only hand out the permission after `veyon-server`
restarts (restart the Veyon Service). MDM can pre-approve Accessibility but not Screen Recording; a PPPC profile can
at most allow standard users to approve it.

## Limitations

- Only the main display is captured, at its resolution in pixels but at most 1920 pixels on the longer side (1920×1200 on a 2880×1800 MacBook display): enough to fill a 1080p screen in demos without the data of a full Retina picture.
- No picture while the session is locked or at the login window (ScreenCaptureKit only sees the user's session).
- Keyboard input is mapped through the Mac's current keyboard layout. Characters the layout cannot type are sent as
  Unicode text, which works for typing but not for shortcuts.

## Trying it without Veyon

`MacOsVncServerCore`, `MacOsScreenCapture` and `MacOsInputInjector` do not use Qt, so the server can be built and run
on its own with Homebrew's `libvncserver` (no password, localhost only). With a `main()` that calls
`MacOsVncServerCore::run()` on a separate thread and `CFRunLoopRun()` on the main thread (the keyboard layout is read
on the main thread):

```bash
clang++ -std=c++20 -fobjc-arc -I. -I"$(brew --prefix)/include" \
  -x objective-c++ MacOsScreenCapture.mm -x c++ MacOsInputInjector.cpp MacOsVncServerCore.cpp main.cpp \
  -L"$(brew --prefix)/lib" -lvncserver -framework ScreenCaptureKit -framework CoreMedia -framework CoreVideo \
  -framework CoreGraphics -framework ApplicationServices -framework Carbon -framework Accelerate -framework Foundation
```

The permissions then belong to the terminal app that started it.
