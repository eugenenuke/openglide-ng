# OpenGLide (Modernized SDL2 Fork)

OpenGLide is a Glide-to-OpenGL wrapper that allows applications and games written for the 3dfx Voodoo series of graphics cards to run on modern hardware.

This specific fork has been modernized to replace legacy X11/SDL 1.2 code with a native **SDL2** backend, enabling better support for modern Linux environments, high-resolution displays, and remote desktop sessions.

## Key Modernization Features

### 1. SDL2 Native Backend
*   Replaced legacy X11/GLX window management with SDL2.
*   Uses `SDL_GL_GetDrawableSize` for accurate pixel-to-window mapping.
*   Full support for **High-DPI (4K/Retina)** displays via `SDL_WINDOW_ALLOW_HIGHDPI`.

### 2. Hardware-Accelerated Scaling
*   Automatically maintains correct Glide aspect ratios (e.g., 4:3).
*   Implements pillarboxing and letterboxing using `glViewport` to prevent image stretching.
*   Dynamic viewport updates when the parent window (e.g., DOSBox) is resized.

### 3. Remote Desktop & Wayland Compatibility
*   Bypasses legacy X11 extensions like `MIT-SHM` and `XF86VidMode` that often crash in remote or Wayland sessions.
*   Verified to work with software rasterizers like `llvmpipe`.

### 4. Raw Terminal Logging
*   Standardized debug output with `
` line endings to ensure correct formatting when the terminal is in raw mode (common during Glide tests).

## Building

### Requirements
*   Build tools (gcc, g++, make, autoconf, automake, libtool)
*   SDL2 Development libraries (`libsdl2-dev`)
*   OpenGL/GLU Development libraries (`libgl1-mesa-dev`, `libglu1-mesa-dev`)

### Instructions
```bash
bash ./bootstrap
mkdir build && cd build
../configure --enable-sdl
make -j$(nproc)
```
The resulting libraries will be in `build/.libs/libglide2x.so` and `build/.libs/libglide3x.so`.

## Usage with DOSBox-Staging

1.  Point your `LD_LIBRARY_PATH` to the built library:
    ```bash
    export LD_LIBRARY_PATH=/path/to/openglide/build/.libs:$LD_LIBRARY_PATH
    ```
2.  Enable Glide in your `dosbox.conf`:
    ```ini
    [glide]
    glide=true
    ```
3.  Launch DOSBox. It will automatically load the modernized OpenGlide and attach it to the SDL2 window.

## Testing
To verify the installation, you can compile and run the 3dfx SDK tests:
*   **Test 05:** Basic Z-buffering (Static triangles).
*   **Test 14:** Texture mapping and rotation.
*   **Test 25:** Anti-aliased geometry stress test.
*   **Test 26:** Linear Frame Buffer (LFB) read/write verification.

Refer to the session history for specific `gcc` compilation strings for these tests.
