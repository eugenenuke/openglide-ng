//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*     SDL specific functions for handling display window
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//*               Linux version by Simon White
//**************************************************************
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef C_USE_SDL

#include <stdlib.h>
#include <math.h>

#if defined(__linux__) || defined(__darwin__)
#include <dlfcn.h>
#else
#include <windows.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "GlOgl.h"

#include "platform/window.h"

static struct gamma_ramp {
    uint16_t red[256];
    uint16_t blue[256];
    uint16_t green[256];
} old_ramp;
static bool ramp_stored;

static SDL_Window *window;
static SDL_GLContext context;
static bool self_ctx, self_wnd;

bool InitialiseOpenGLWindow(FxU wnd, int x, int y, int width, int height)
{
    self_wnd = false;
    self_ctx = false;

    // Standardized logging for raw terminals
    fprintf(stderr, "Info: InitialiseOpenGLWindow(wnd=0x%lx, res=%dx%d)\r\n", (unsigned long)wnd, width, height);

    // 1. Try to hijack the existing SDL2 context if DOSBox already has one active
    window = SDL_GL_GetCurrentWindow();
    context = SDL_GL_GetCurrentContext();

    if (window && context) {
        fprintf(stderr, "Info: Hijacking existing SDL2 OpenGL context\r\n");
    } else {
        // 2. No existing context, we need to attach or create
        if (!wnd) {
            const char *title = "SDL2-OpenGLide";
            uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
            if (UserConfig.InitFullScreen) {
                flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            }
            window = SDL_CreateWindow(title, x, y, width, height, flags);
            self_wnd = (window != nullptr);
        } else {
            // Check if wnd is likely a pointer or a handle
            if (wnd > 0xFFFFFFFFUL) {
                window = (SDL_Window *)wnd;
            } else {
                // It's a native handle, try to wrap it (risky with sdl2-compat/SDL3)
                SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
                window = SDL_CreateWindowFrom((const void *)(uintptr_t)wnd);
                self_wnd = (window != nullptr);
            }
        }

        if (!window) {
            fprintf(stderr, "Error: Could not obtain SDL window\r\n");
            return false;
        }

        // 3. Create context if we don't have one hijacked
        if (!context) {
            SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
            SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
            
            if (UserConfig.SamplesMSAA) {
                SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
                SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, UserConfig.SamplesMSAA);
            }

            context = SDL_GL_CreateContext(window);
            self_ctx = (context != nullptr);
        }
    }

    if (!context) {
        fprintf(stderr, "Error: GL Context Error: %s\r\n", SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(window, context);

    int drawable_w, drawable_h;
    SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);

    int cRedBits, cGreenBits, cBlueBits, cAlphaBits, cDepthBits, cStencilBits,
        cAuxBuffers, nSamples[2], has_sRGB = UserConfig.FramebufferSRGB;

    SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &cRedBits);
    SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &cGreenBits);
    SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &cBlueBits);
    SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &cAlphaBits);
    SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &cDepthBits);
    SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &cStencilBits);
    SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS, &nSamples[0]);
    SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &nSamples[1]);
    glGetIntegerv(GL_AUX_BUFFERS, &cAuxBuffers);

    fprintf(stderr, "Info: %s OpenGL %s\r\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    fprintf(stderr, "Info: Pixel Format RGBA%d%d%d%d D%2dS%d nAux %d nSamples %d %d %s\r\n",
            cRedBits, cGreenBits, cBlueBits, cAlphaBits, cDepthBits, cStencilBits,
            cAuxBuffers, nSamples[0], nSamples[1], (has_sRGB)? "sRGB":"");
    fprintf(stderr, "Info: Drawable Size: %dx%d\r\n", drawable_w, drawable_h);

    // Calculate scaling to maintain aspect ratio
    float target_aspect = (float)width / (float)height;
    float actual_aspect = (float)drawable_w / (float)drawable_h;

    if (actual_aspect > target_aspect) {
        // Window is wider than needed - pillarbox
        OpenGL.WindowHeight = drawable_h;
        OpenGL.WindowWidth = (int)(drawable_h * target_aspect);
        OpenGL.WindowOffset = (drawable_w - OpenGL.WindowWidth) / 2;
    } else {
        // Window is taller than needed - letterbox
        OpenGL.WindowWidth = drawable_w;
        OpenGL.WindowHeight = (int)(drawable_w / target_aspect);
        OpenGL.WindowOffset = 0;
    }

    // Set the viewport based on calculated dimensions
    glViewport(OpenGL.WindowOffset, (drawable_h - OpenGL.WindowHeight) / 2, 
               OpenGL.WindowWidth, OpenGL.WindowHeight);

    if (has_sRGB)
        glEnable(GL_FRAMEBUFFER_SRGB);

    if (cDepthBits > 16)
        UserConfig.PrecisionFix = false;

    for (int i = 0; i < 0x100; i++) {
        old_ramp.red[i]   = (uint16_t)(((i << 8) | i) & 0xFFFFU);
        old_ramp.green[i] = (uint16_t)(((i << 8) | i) & 0xFFFFU);
        old_ramp.blue[i]  = (uint16_t)(((i << 8) | i) & 0xFFFFU);
    }
    ramp_stored = true;

    return true;
}

void FinaliseOpenGLWindow(void)
{
    int has_sRGB = UserConfig.FramebufferSRGB;

    if ( ramp_stored )
        SetGammaTable(&old_ramp);
    if ( has_sRGB )
        glDisable(GL_FRAMEBUFFER_SRGB);

    SetSwapInterval(-1);

    if ( self_ctx ) {
        if (SDL_GL_MakeCurrent(window, NULL) == 0)
            SDL_GL_DeleteContext(context);
    }
    if ( self_wnd )
        SDL_DestroyWindow(window);
}

void SetGammaTable(void *ramp)
{
    if (window) {
        struct gamma_ramp *r = (struct gamma_ramp *)ramp;
        SDL_SetWindowGammaRamp(window, r->red, r->green, r->blue);
    }
}

void GetGammaTable(void *ramp)
{
    if (window) {
        struct gamma_ramp *r = (struct gamma_ramp *)ramp;
        SDL_GetWindowGammaRamp(window, r->red, r->green, r->blue);
    }
}

void SetGamma(float value) {}
void RestoreGamma()
{
    if ( ramp_stored )
        SetGammaTable(&old_ramp);
}

bool SetScreenMode(int &xsize, int &ysize) { return true; }
void ResetScreenMode() {}

void SetSwapInterval(const int interval)
{
    SDL_GL_SetSwapInterval(interval);
}

void SwapBuffers()
{
    SDL_Event e;
    while(SDL_PollEvent(&e));
    if (UserConfig.swap12) {
        void (*glSwapFunc)(void) = (void (*)(void))UserConfig.swap12;
        glSwapFunc();
        return;
    }
    SDL_GL_SwapWindow(window);
}

#endif // C_USE_SDL
