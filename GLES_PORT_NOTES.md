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

### 4. No Mipmaps
GLES 1.1 mipmap generation is unreliable. Using `GL_LINEAR` filtering only.

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

```bash
# Set up toolchain
export PATH="/home/stark/arm-2011.03/bin:$PATH"

# Configure (if needed)
./configure --host=arm-none-linux-gnueabi --enable-gles \
  --with-sdl-prefix=/home/stark/HPwebOS/PDK \
  --prefix=/media/internal/warzone

# Build
make -j4

# Deploy
scp src/warzone2100 root@<touchpad-ip>:/media/internal/warzone/bin/
```

---

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
