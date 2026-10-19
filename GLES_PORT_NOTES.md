# Warzone 2100 v2.3 - OpenGL ES 1.1 Port for HP TouchPad (webOS)

## Overview
This document tracks all changes made to port Warzone 2100 from desktop OpenGL to OpenGL ES 1.1 for the HP TouchPad running webOS.

**Target Platform:** HP TouchPad, webOS 3.x
**Toolchain:** CodeSourcery 2011.03 (`/home/stark/arm-2011.03`)
**Game Data Path:** `/media/internal/warzone/`

---

## Current Status

### Working
- PDL + SDL OpenGL initialization (proper webOS compositor integration)
- 2D menu textures and backgrounds
- 3D model textures (vehicles, buildings)
- Basic 3D rendering
- Immediate mode emulation via vertex arrays
- Terrain textures (fixed mipmap loading issue)
- Text rendering / menu fonts (fixed texture matrix issue)
- Touch input without flickering
- Card view transitions work correctly

### Not Working / Known Issues
- None! Port is functional.

### Notes
- Touch input works as mouse (SDL handles this automatically)
- Map scrolling: drag and hold at screen edges

---

## Files Created

### `lib/ivis_opengl/gles_compat.h`
GLES 1.1 compatibility header that replaces GLee.h for GLES builds.

**Key features:**
- Immediate mode emulation macros (`glBegin` -> `glesBegin`, etc.)
- GL_QUADS and GL_QUAD_STRIP definitions (not in GLES)
- Double-to-float function mappings (`glOrtho` -> `glOrthof`, etc.)
- GL_CLAMP -> GL_CLAMP_TO_EDGE mapping
- GLU function replacements (`gluErrorString`, `gluBuild2DMipmaps`, `gluScaleImage`)
- GLEE extension stubs (all disabled for GLES)
- glPushAttrib/glPopAttrib emulation
- Stencil operation stubs (two-sided stencil not in GLES 1.1)
- glPolygonMode stub (not in GLES)
- glGetDoublev -> glGetFloatv wrapper

### `lib/ivis_opengl/gles_compat.c`
Implementation of GLES compatibility layer.

**Key features:**
- `GLESImmediateBuffer` - Batches vertices for immediate mode emulation
- `glesBegin()`/`glesEnd()` - Start/end immediate mode batch
- `glesVertex2f/2i/3f/3fv()` - Vertex submission
- `glesColor3f/4f/4ub/4ubv()` - Color submission
- `glesTexCoord2f/2fv()` - Texture coordinate submission
- `glesNormal3fv()` - Normal submission
- `glesPushAttrib()`/`glesPopAttrib()` - Attribute stack emulation
- `glesBuild2DMipmaps()` - Simple texture upload (no actual mipmaps)
- `glesScaleImage()` - Basic nearest-neighbor image scaling
- GL_QUADS to GL_TRIANGLES conversion in `glesFlush()`

### `lib/ivis_opengl/gl_compat.h`
Wrapper header that includes either GLee.h (desktop) or gles_compat.h (GLES).

---

## Files Modified

### `lib/ivis_opengl/screen.c`
**EGL Initialization:**
- Added manual EGL setup bypassing SDL's broken EGL handling on webOS
- EGL display, surface, and context creation
- EGL buffer swap implementation
- Swap interval set to 0 (vsync off - attempted flicker fix)

**Texture Matrix:**
- Enabled `glScalef(1/256, 1/256, 1)` on texture matrix for GLES
- This scales 0-256 texture coordinates to 0-1 range (matches desktop behavior)

**Double Buffer Clearing:**
- Added clearing of both front and back buffers on init to reduce initial flicker

### `lib/ivis_opengl/piestate.c`
- Disabled fog completely for GLES (`pie_SetFogStatus` returns early)
- Fixed `pie_SetTexturePage` to handle invalid texture page state (-999)
- Added `glEnable(GL_TEXTURE_2D)` when coming from invalid state

### `lib/ivis_opengl/piedraw.c`
- Disabled lighting for GLES (`BOOL light = false`)
- Disabled shadows for GLES (stencil buffer shadows not available)
- Added `glEnable(GL_TEXTURE_2D)` before 3D model drawing

### `lib/ivis_opengl/tex.c`
- Changed texture filter from `GL_LINEAR_MIPMAP_LINEAR` to `GL_LINEAR` for GLES
- Mipmaps not reliably generated on GLES 1.1

### `lib/ivis_opengl/textdraw.c`
- Implemented bitmap font renderer for GLES (GLC library not available)
- Creates 256x256 font texture atlas on init
- Renders ASCII 32-127 characters
- **NOTE:** Currently broken after texture matrix changes - needs fix

### `lib/ivis_opengl/piematrix.c`
- `pie_TranslateTextureEnd()` restores texture matrix with 1/256 scaling

### `src/texture.c` (Terrain Textures)
- Added `#ifndef USE_GLES` guards around:
  - `GL_TEXTURE_BASE_LEVEL` / `GL_TEXTURE_MAX_LEVEL` (not in GLES 1.1)
- Changed terrain texture to use `GL_LINEAR` instead of `GL_LINEAR_MIPMAP_LINEAR`
- Force mipmap level 0 and GL_RGBA format for GLES

### `configure.ac` / Build System
- Added `--enable-gles` configure option
- Added `-DUSE_GLES` compiler flag
- Link against `-lGLES_CM -lEGL` instead of `-lGL`
- Removed GLee.c from GLES builds

---

## Key Technical Decisions

### 1. Immediate Mode Emulation
GLES 1.1 doesn't support `glBegin`/`glEnd`. Solution: batch vertices into arrays and submit via `glDrawArrays`/`glDrawElements`.

### 2. Texture Coordinate Scaling
Warzone uses 0-256 texture coordinates throughout. Desktop OpenGL scales these via texture matrix.
- **Attempted:** Manual scaling in `glesTexCoord2f` - caused issues
- **Current:** Use texture matrix scaling (same as desktop) - works for 3D models

### 3. GL_QUADS Conversion
GLES doesn't support GL_QUADS. Converted to triangles: quad vertices 0,1,2,3 become triangles (0,1,2) and (0,2,3).

### 4. Mipmaps
Terrain: the tile sets ship in 128, 64, 32 and 16 pixel versions, which load
as the first four mip levels of the terrain atlas (`newPage` now honours the
level). GLES 1.1 has no `GL_TEXTURE_MAX_LEVEL`, so `texLoad` builds the
remaining levels down to 1x1 from the smallest set. Model textures use
`GL_GENERATE_MIPMAP`. Filtering is `GL_LINEAR_MIPMAP_NEAREST`. Without
mipmaps, the minified 2048x2048 terrain atlas thrashed the texture cache.

### 5. No Stencil Shadows
Shadow volumes require stencil buffer operations not available in GLES 1.1. Shadows disabled.

### 6. No Fog
Fog disabled for GLES to simplify rendering pipeline during debugging.

---

## Remaining Issues

### 1. Terrain Textures Wrong - FIXED
- **Root cause:** In `texLoad()`, the mipmap loading loop called `newPage()` for each
  mipmap level (0-3). On GLES, `newPage()` always creates texture level 0 regardless
  of the 'level' parameter. Each iteration recreated level 0 with smaller dimensions,
  overwriting the previous content. After all iterations, the texture contained only
  the smallest (16x16) mipmap tiles, but `tileTexInfo[]` had UV coords for the largest
  (128x128/2048x2048) layout.
- **Fix:** Added `#ifdef USE_GLES` block in `texLoad()` to set `mipmap_levels = 1`,
  ensuring only the base mipmap level is loaded. Since GLES uses `GL_LINEAR` filtering
  (not `GL_LINEAR_MIPMAP_LINEAR`), mipmaps aren't needed anyway.

### 2. Menu Font Not Displaying - FIXED
- **Root cause:** Multiple issues in the GLES bitmap font renderer:
  1. Texture matrix not reset to identity (global 1/256 scale affected font UVs)
  2. Depth test and alpha test interfering with 2D text rendering
  3. Color not set after glBegin() (GLES compat layer resets hasColor flag)
- **Fix:** In `iV_DrawTextRotated()` in `textdraw.c`:
  - Push/pop texture matrix with identity for font rendering
  - Disable depth test and alpha test during font drawing
  - Set glColor4f after glBegin() to ensure color array is populated
  - Properly save/restore GL state

### 3. Flickering - FIXED
- Occurs on touch input and card view (webOS multitasking)
- **Root cause:** Manual EGL initialization with `(EGLNativeWindowType)0` bypassed
  the webOS window manager/compositor entirely. This caused:
  1. Game rendering fullscreen even when in card view
  2. Touch events causing flicker (input via SDL, rendering on separate surface)
  3. No proper integration with webOS compositor

- **Fix attempts:**
  - **v2 (Buffer preserved):** Tried `EGL_BUFFER_PRESERVED` + vsync + glFinish() - Still flickers
  - **v3 (SDL_OPENGLES):** Tried `SDL_OPENGLES | SDL_FULLSCREEN` to let SDL manage EGL
    - **FAILED:** "Could not create EGL context" crash - SDL's EGL support is broken on webOS
  - **v4 (Native window from SDL):** Keep manual EGL but try to get native window from SDL
    - Use `SDL_GetWMInfo()` to get native window handle - Still flickers
  - **v5 (PDL Integration):** Use PDL library as HP's official samples do
    - **Key discovery:** HP's official PDK samples use `PDL_Init()` + `SDL_OPENGL` +
      `SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, X)` - SDL handles EGL internally
    - Changed `video_flags` to `SDL_OPENGL | SDL_FULLSCREEN` (not SDL_OPENGLES)
    - Call `PDL_Init(0)` BEFORE `SDL_SetVideoMode()` for compositor integration
    - Set `SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1)` for GLES 1.1
    - Removed all manual EGL code (initEGL, shutdownEGL, etc.)
    - Use `SDL_GL_SwapBuffers()` for buffer swap
    - Added `-lpdl` to link flags in configure.ac
    - **WORKS!** No more flickering on touch or card view

---

## Testing Findings (User Reports)

1. **Initial state:** Black screen, nothing rendering
2. **After EGL fix:** Basic rendering works
3. **After immediate mode emulation:** 2D elements render
4. **After texture matrix scaling:** 3D model textures work correctly
5. **After terrain fix:** Terrain textures now display correctly
6. **After font fix:** Menu text now displays correctly
7. **Current:** Vehicles, buildings, terrain, and fonts rendering correctly; flickering persists

---

## Build Instructions

Builds run in WSL with the CodeSourcery 2011.03 toolchain and the Palm PDK.
The 32-bit toolchain cannot read Windows drives, so the scripts mirror the
tree to an ext4 disk and build there. Paths default to the author's setup and
can be overridden through environment variables (see each script).

```bash
# once: static ogg, vorbis, theora, physfs and popt, optimised for the Cortex-A8
pkg/webos/build-deps.sh

# the game; the stripped binary lands in pkg/webos/warzone2100
pkg/webos/build-touchpad.sh
```

`build-touchpad.sh` configures with `--enable-debug=no` and
`-O2 -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp`. Warzone's configure
defaults to a debug build with no optimisation, which is how the first
releases were built.

Game data (`base.wz`, `mp.wz`) goes in `/media/internal/warzone/`, which is
also where the config and saves are written.

---

## Performance (October 2026)

From 11-12 FPS to about 25-35 FPS in a skirmish. Each item below was measured
on the TouchPad with the profiler described afterwards.

- **Release build.** The previous binaries were `-O0` debug builds with
  assertions enabled.
- **Config file read.** The webOS branch of `initialize_ConfigDir` never added
  the write directory to the search path, so `config` was never read: every
  start used the defaults and saved them over the file.
- **Draw call merging** (`gles_compat.c`). The driver spends about 24 us of
  CPU time and 23 us of GPU time per draw call, however small (measured: 300
  draws of 6 vertices cost 7 ms to submit, 1 draw of 1800 vertices 0.4 ms).
  Triangles from `glBegin`/`glEnd` and from models therefore collect in one
  batch, moved into eye space on the CPU, and draw in one call when GL state
  really changes. Every state-changing GL call goes through a wrapper that
  draws the batch first and skips changes that change nothing. About 300 draws
  a frame became about 80.
- **Model texture atlas** (`tex.c`). Model pages up to 256x256 share one
  2048x2048 texture, so moving between a droid's body, propulsion and weapon
  no longer changes texture and breaks the batch. `pie_SetTexturePage` sets
  the texture coordinate transform for the page's slot. The skybox (`page-25`)
  keeps its own texture because it repeats. Two-sided polygons go into the
  batch twice, once per winding, instead of switching culling off.
- **Models** go to the batch whole instead of one draw per polygon.
- **Terrain** (`piedraw.c`): four shared corners per tile in one interleaved
  16-byte vertex (shorts; texcoords in 1/32768ths through the texture matrix),
  and indices only for the tiles drawn this frame, in one draw call. This
  replaced six float vertices per tile, one draw per row and clearing the
  whole array every frame.
- **Water** triangles are drawn together (`pie_FlushWater`).
- **Text** is drawn a string at a time.
- **Fog of war** (`avUpdateTiles`) visits every map tile each frame; the
  per-tile uplink, god mode and alliance checks are now done once per frame,
  and tiles are walked in memory order.
- **Mouse picking** checks each tile's bounding box before the exact test.

### Touch controls (2.3.1)

`intAddReticule` (hci.c) adds MENU in the top left and one cluster in the
bottom right: a LEFT CLICK / RIGHT CLICK toggle (the `RightClickOrders`
setting), a SCROLL / ROTATE toggle (`setTouchRotateMode`, display.c: in rotate
mode the arrow buttons call `kf_RotateLeft/Right` and `kf_PitchBack/Forward`),
then the arrows and zoom as 80x80 buttons. The minimap moves to the top right
(`radarSize`, radar.c). The toggles show the current mode and keep it when the
reticule is rebuilt.

### Profiling

Run with `WZ_PROFILE=1` in the environment for a line on stderr every 100
frames: milliseconds per stage (gpu, swap, logic, sky, terrain, effects,
tdraw, water, objects, hud, ui), draw calls and their time, and why batches
were drawn. While profiling, the GPU is waited for before each swap so that its
time shows on its own, which lowers the frame rate somewhat.

For a function-level profile, build with `DEBUG=profile BUILD=<dir>
OUT=warzone2100-gprof pkg/webos/build-touchpad.sh`, play, quit through the
menus (gmon.out is written at exit, in the current directory), and run the
toolchain's `arm-none-linux-gnueabi-gprof` on the unstripped binary.

### What is left

Per frame, roughly: terrain building 4 ms, terrain projection 3 ms, interface
3.5 ms, effects 2 ms, and a GPU cost of about 8-9 ms that is there even for an
empty frame. That last one is probably the webOS compositor; 16-bit colour
(RGB565) is already in use.

## Next Steps

1. **Fix flickering:** Investigate webOS event handling and EGL context
2. **Implement touch controls:** TouchPad has no mouse/keyboard - need touch-based map scrolling (drag to pan, pinch to zoom?)

---

## File Checklist for GLES Port

- [x] `lib/ivis_opengl/gles_compat.h` - Created
- [x] `lib/ivis_opengl/gles_compat.c` - Created
- [x] `lib/ivis_opengl/gl_compat.h` - Created
- [x] `lib/ivis_opengl/screen.c` - Modified (EGL, texture matrix)
- [x] `lib/ivis_opengl/piestate.c` - Modified (fog, texture page)
- [x] `lib/ivis_opengl/piedraw.c` - Modified (lighting, shadows)
- [x] `lib/ivis_opengl/tex.c` - Modified (filtering)
- [x] `lib/ivis_opengl/textdraw.c` - Modified (bitmap font)
- [x] `lib/ivis_opengl/piematrix.c` - Reviewed (texture matrix restore)
- [x] `src/texture.c` - Modified (terrain texture params)
- [x] `configure.ac` - Modified (GLES option)
- [x] `lib/ivis_opengl/Makefile.am` - Modified (gles_compat files)
