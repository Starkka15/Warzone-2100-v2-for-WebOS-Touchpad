# Warzone 2100 v2 for webOS TouchPad

Port of the classic RTS game Warzone 2100 to HP webOS TouchPad with OpenGL ES rendering.

## Installing

Install `org.wz2100.warzone2100_<version>_all.ipk` from the releases with
Preware, WebOS Quick Install or `palm-install`. On install the game data moves
to `/media/internal/warzone/`, where the settings and saves also live.

## Touch controls

- **MENU** (top left): the in-game menu, since there is no Escape key.
- Bottom right:
  - **LEFT CLICK / RIGHT CLICK**: whether a tap on the map acts as a left click
    (select) or a right click (orders). Saved with the settings.
  - **SCROLL / ROTATE**: whether the arrows scroll the map or turn and tilt the
    camera.
  - **< ^ >** and **v**: scroll, or rotate and pitch.
  - **+** and **-**: zoom.
- The minimap is in the top right.

## Building

In WSL, with the CodeSourcery 2011.03 ARM toolchain and the Palm PDK:

```bash
pkg/webos/build-deps.sh       # once: static ogg, vorbis, theora, physfs, popt
pkg/webos/build-touchpad.sh   # the game; leaves pkg/webos/warzone2100
```

Then, on Windows with the Palm SDK, package it with the stock game data
(`base.wz`, `mp.wz`, `music/`; the `warzone` folder of an earlier release will do):

```powershell
pkg\webos\make-ipk.ps1 -DataDir <data folder>
```

See [GLES_PORT_NOTES.md](GLES_PORT_NOTES.md) for how the port works, the
performance work, and profiling.

## License

GPL-2.0
