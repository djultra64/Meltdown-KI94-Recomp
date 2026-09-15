#include "ki/pc_window.h"

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SDL's public objects are opaque; no SDL development headers are required. */
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
typedef struct SdlRect { int x, y, w, h; } SdlRect;
typedef struct SdlKeysym {
    int scancode, keycode;
    uint16_t modifiers;
    uint32_t unused;
} SdlKeysym;
typedef struct SdlKeyboardEvent {
    uint32_t type,timestamp,window_id;
    uint8_t state,repeat,padding2,padding3;
    SdlKeysym keysym;
} SdlKeyboardEvent;

/* Only the event type is inspected. Extra space preserves SDL2's event ABI. */
typedef union SdlEvent {
    uint32_t type;
    SdlKeyboardEvent key;
    uint8_t storage[64];
} SdlEvent;

_Static_assert(offsetof(SdlKeyboardEvent, keysym) == 16,
               "SDL2 keyboard event ABI changed");
_Static_assert(sizeof(SdlEvent)>=56,"SDL2 event union storage is too small");

enum {
    SDL_INIT_VIDEO_VALUE = 0x00000020u,
    SDL_WINDOW_SHOWN_VALUE = 0x00000004u,
    SDL_WINDOW_RESIZABLE_VALUE = 0x00000020u,
    SDL_WINDOW_ALLOW_HIGHDPI_VALUE = 0x00002000u,
    SDL_WINDOW_FULLSCREEN_DESKTOP_VALUE = 0x00001001u,
    SDL_RENDERER_SOFTWARE_VALUE = 0x00000001u,
    SDL_RENDERER_ACCELERATED_VALUE = 0x00000002u,
    SDL_RENDERER_PRESENTVSYNC_VALUE = 0x00000004u,
    SDL_TEXTUREACCESS_STREAMING_VALUE = 1,
    SDL_PIXELFORMAT_ARGB8888_VALUE = 0x16362004u,
    SDL_QUIT_EVENT_VALUE = 0x100u,
    SDL_KEYDOWN_EVENT_VALUE = 0x300u,
    SDLK_F11_VALUE = 1073741892,
    SDL_WINDOW_POSITION_CENTERED = 0x2fff0000u
};

typedef int (*SdlInitFn)(uint32_t);
typedef void (*SdlQuitFn)(void);
typedef const char *(*SdlGetErrorFn)(void);
typedef int (*SdlSetHintFn)(const char *, const char *);
typedef SDL_Window *(*SdlCreateWindowFn)(const char *, int, int, int, int,
                                         uint32_t);
typedef void (*SdlDestroyWindowFn)(SDL_Window *);
typedef SDL_Renderer *(*SdlCreateRendererFn)(SDL_Window *, int, uint32_t);
typedef void (*SdlDestroyRendererFn)(SDL_Renderer *);
typedef SDL_Texture *(*SdlCreateTextureFn)(SDL_Renderer *, uint32_t, int, int,
                                           int);
typedef void (*SdlDestroyTextureFn)(SDL_Texture *);
typedef int (*SdlUpdateTextureFn)(SDL_Texture *, const void *, const void *,
                                  int);
typedef int (*SdlRenderCopyFn)(SDL_Renderer *, SDL_Texture *, const void *,
                               const void *);
typedef void (*SdlRenderPresentFn)(SDL_Renderer *);
typedef int (*SdlGetRendererOutputSizeFn)(SDL_Renderer *,int *,int *);
typedef int (*SdlSetRenderDrawColorFn)(SDL_Renderer *,uint8_t,uint8_t,uint8_t,uint8_t);
typedef int (*SdlRenderClearFn)(SDL_Renderer *);
typedef int (*SdlSetWindowFullscreenFn)(SDL_Window *,uint32_t);
typedef uint32_t (*SdlGetWindowFlagsFn)(SDL_Window *);
typedef int (*SdlPollEventFn)(SdlEvent *);
typedef void (*SdlDelayFn)(uint32_t);

typedef struct SdlFunctions {
    SdlInitFn init;
    SdlQuitFn quit;
    SdlGetErrorFn get_error;
    SdlSetHintFn set_hint;
    SdlCreateWindowFn create_window;
    SdlDestroyWindowFn destroy_window;
    SdlCreateRendererFn create_renderer;
    SdlDestroyRendererFn destroy_renderer;
    SdlCreateTextureFn create_texture;
    SdlDestroyTextureFn destroy_texture;
    SdlUpdateTextureFn update_texture;
    SdlRenderCopyFn render_copy;
    SdlRenderPresentFn render_present;
    SdlGetRendererOutputSizeFn get_renderer_output_size;
    SdlSetRenderDrawColorFn set_render_draw_color;
    SdlRenderClearFn render_clear;
    SdlSetWindowFullscreenFn set_window_fullscreen;
    SdlGetWindowFlagsFn get_window_flags;
    SdlPollEventFn poll_event;
    SdlDelayFn delay;
} SdlFunctions;

struct KiPcWindow {
    void *library;
    SdlFunctions sdl;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int logical_width;
    int logical_height;
    KiPcFitMode fit_mode;
    uint32_t decoration_argb;
};

static void set_error(char *destination,
                      size_t capacity,
                      const char *message,
                      const char *detail)
{
    if (destination == NULL || capacity == 0) {
        return;
    }
    if (detail == NULL || detail[0] == '\0') {
        (void)snprintf(destination, capacity, "%s", message);
    } else {
        (void)snprintf(destination, capacity, "%s: %s", message, detail);
    }
}

static int load_function(void *library,
                         const char *name,
                         void *function_storage,
                         size_t storage_size)
{
    void *symbol = dlsym(library, name);
    if (symbol == NULL || storage_size != sizeof(symbol)) {
        return 0;
    }

    /* POSIX dlsym returns an object pointer; memcpy avoids nonportable casts. */
    memcpy(function_storage, &symbol, storage_size);
    return 1;
}

#define LOAD_SDL_FUNCTION(TABLE, LIBRARY, FIELD, NAME)                       \
    do {                                                                     \
        if (!load_function((LIBRARY), (NAME), &(TABLE)->FIELD,              \
                           sizeof((TABLE)->FIELD))) {                        \
            return 0;                                                        \
        }                                                                    \
    } while (0)

static int load_sdl_functions(void *library, SdlFunctions *sdl)
{
    LOAD_SDL_FUNCTION(sdl, library, init, "SDL_Init");
    LOAD_SDL_FUNCTION(sdl, library, quit, "SDL_Quit");
    LOAD_SDL_FUNCTION(sdl, library, get_error, "SDL_GetError");
    LOAD_SDL_FUNCTION(sdl, library, set_hint, "SDL_SetHint");
    LOAD_SDL_FUNCTION(sdl, library, create_window, "SDL_CreateWindow");
    LOAD_SDL_FUNCTION(sdl, library, destroy_window, "SDL_DestroyWindow");
    LOAD_SDL_FUNCTION(sdl, library, create_renderer, "SDL_CreateRenderer");
    LOAD_SDL_FUNCTION(sdl, library, destroy_renderer, "SDL_DestroyRenderer");
    LOAD_SDL_FUNCTION(sdl, library, create_texture, "SDL_CreateTexture");
    LOAD_SDL_FUNCTION(sdl, library, destroy_texture, "SDL_DestroyTexture");
    LOAD_SDL_FUNCTION(sdl, library, update_texture, "SDL_UpdateTexture");
    LOAD_SDL_FUNCTION(sdl, library, render_copy, "SDL_RenderCopy");
    LOAD_SDL_FUNCTION(sdl, library, render_present, "SDL_RenderPresent");
    LOAD_SDL_FUNCTION(sdl, library, get_renderer_output_size, "SDL_GetRendererOutputSize");
    LOAD_SDL_FUNCTION(sdl, library, set_render_draw_color, "SDL_SetRenderDrawColor");
    LOAD_SDL_FUNCTION(sdl, library, render_clear, "SDL_RenderClear");
    LOAD_SDL_FUNCTION(sdl, library, set_window_fullscreen, "SDL_SetWindowFullscreen");
    LOAD_SDL_FUNCTION(sdl, library, get_window_flags, "SDL_GetWindowFlags");
    LOAD_SDL_FUNCTION(sdl, library, poll_event, "SDL_PollEvent");
    LOAD_SDL_FUNCTION(sdl, library, delay, "SDL_Delay");
    return 1;
}

#undef LOAD_SDL_FUNCTION

static void *open_sdl_library(void)
{
    static const char *const candidates[] = {
        "libSDL2-2.0.so.0",
        "libSDL2.so.0",
    };
    for (size_t index = 0; index < sizeof(candidates) / sizeof(candidates[0]);
         index++) {
        void *library = dlopen(candidates[index], RTLD_NOW | RTLD_LOCAL);
        if (library != NULL) {
            return library;
        }
    }
    return NULL;
}

KiPcWindow *ki_pc_window_create(const char *title,
                                int logical_width,
                                int logical_height,
                                int display_scale,
                                char *error,
                                size_t error_capacity)
{
    const KiPcWindowOptions options={logical_width,logical_height,display_scale,
                                     KI_PC_FIT_INTEGER,UINT32_C(0xff101018),0};
    return ki_pc_window_create_with_options(title,&options,error,error_capacity);
}

KiPcWindow *ki_pc_window_create_with_options(const char *title,
                                              const KiPcWindowOptions *options,
                                              char *error,
                                              size_t error_capacity)
{
    if (title == NULL || options == NULL || options->logical_width <= 0 ||
        options->logical_height <= 0 || options->display_scale <= 0 ||
        options->logical_width > INT_MAX / options->display_scale ||
        options->logical_height > INT_MAX / options->display_scale ||
        (options->fit_mode != KI_PC_FIT_INTEGER && options->fit_mode != KI_PC_FIT_ASPECT)) {
        set_error(error, error_capacity, "invalid window dimensions", NULL);
        return NULL;
    }

    KiPcWindow *result = calloc(1, sizeof(*result));
    if (result == NULL) {
        set_error(error, error_capacity, "could not allocate PC window", NULL);
        return NULL;
    }

    result->library = open_sdl_library();
    if (result->library == NULL) {
        set_error(error, error_capacity, "could not load SDL2", dlerror());
        free(result);
        return NULL;
    }
    if (!load_sdl_functions(result->library, &result->sdl)) {
        set_error(error, error_capacity, "SDL2 is missing a required symbol",
                  dlerror());
        dlclose(result->library);
        free(result);
        return NULL;
    }
    if (result->sdl.init(SDL_INIT_VIDEO_VALUE) != 0) {
        set_error(error, error_capacity, "SDL video initialization failed",
                  result->sdl.get_error());
        dlclose(result->library);
        free(result);
        return NULL;
    }

    /* "0" requests nearest-neighbor texture expansion for pixel-accurate art. */
    (void)result->sdl.set_hint("SDL_RENDER_SCALE_QUALITY", "0");
    result->window = result->sdl.create_window(
        title, (int)SDL_WINDOW_POSITION_CENTERED,
        (int)SDL_WINDOW_POSITION_CENTERED,
        options->logical_width * options->display_scale,
        options->logical_height * options->display_scale,
        SDL_WINDOW_SHOWN_VALUE|SDL_WINDOW_RESIZABLE_VALUE|SDL_WINDOW_ALLOW_HIGHDPI_VALUE);
    if (result->window == NULL) {
        set_error(error, error_capacity, "SDL window creation failed",
                  result->sdl.get_error());
        ki_pc_window_destroy(result);
        return NULL;
    }

    result->renderer = result->sdl.create_renderer(
        result->window, -1, SDL_RENDERER_ACCELERATED_VALUE);
    if (result->renderer == NULL) {
        result->renderer = result->sdl.create_renderer(
            result->window, -1, SDL_RENDERER_SOFTWARE_VALUE);
    }
    if (result->renderer == NULL) {
        set_error(error, error_capacity, "SDL renderer creation failed",
                  result->sdl.get_error());
        ki_pc_window_destroy(result);
        return NULL;
    }

    result->texture = result->sdl.create_texture(
        result->renderer, SDL_PIXELFORMAT_ARGB8888_VALUE,
        SDL_TEXTUREACCESS_STREAMING_VALUE, options->logical_width, options->logical_height);
    if (result->texture == NULL) {
        set_error(error, error_capacity, "SDL texture creation failed",
                  result->sdl.get_error());
        ki_pc_window_destroy(result);
        return NULL;
    }

    result->logical_width = options->logical_width;
    result->logical_height = options->logical_height;
    result->fit_mode = options->fit_mode;
    result->decoration_argb = options->decoration_argb;
    if (options->fullscreen_desktop &&
        result->sdl.set_window_fullscreen(result->window,
            SDL_WINDOW_FULLSCREEN_DESKTOP_VALUE) != 0) {
        set_error(error, error_capacity, "SDL fullscreen failed",
                  result->sdl.get_error());
        ki_pc_window_destroy(result);
        return NULL;
    }
    return result;
}

int ki_pc_window_process_events(KiPcWindow *window)
{
    if (window == NULL) {
        return 0;
    }

    SdlEvent event;
    while (window->sdl.poll_event(&event)) {
        if (event.type == SDL_QUIT_EVENT_VALUE) {
            return 0;
        }
        if (event.type == SDL_KEYDOWN_EVENT_VALUE && !event.key.repeat &&
            event.key.keysym.keycode == SDLK_F11_VALUE) {
            const uint32_t flags = window->sdl.get_window_flags(window->window);
            const uint32_t mode =
                (flags & SDL_WINDOW_FULLSCREEN_DESKTOP_VALUE) ? 0u :
                SDL_WINDOW_FULLSCREEN_DESKTOP_VALUE;
            (void)window->sdl.set_window_fullscreen(window->window, mode);
        }
    }
    return 1;
}

int ki_pc_window_present(KiPcWindow *window,
                         const uint32_t *argb_pixels,
                         size_t stride_pixels,
                         char *error,
                         size_t error_capacity)
{
    if (window == NULL || argb_pixels == NULL ||
        stride_pixels < (size_t)window->logical_width ||
        stride_pixels > (size_t)INT32_MAX / sizeof(*argb_pixels)) {
        set_error(error, error_capacity, "invalid frame presentation", NULL);
        return 0;
    }

    const int pitch = (int)(stride_pixels * sizeof(*argb_pixels));
    if (window->sdl.update_texture(window->texture, NULL, argb_pixels, pitch) !=
        0) {
        set_error(error, error_capacity, "SDL texture update failed",
                  window->sdl.get_error());
        return 0;
    }
    int drawable_width, drawable_height;
    KiPcViewport viewport;
    if (window->sdl.get_renderer_output_size(
            window->renderer, &drawable_width, &drawable_height) != 0 ||
        !ki_pc_viewport_layout(window->logical_width, window->logical_height,
                               drawable_width, drawable_height,
                               window->fit_mode, &viewport)) {
        set_error(error, error_capacity, "SDL drawable size failed",
                  window->sdl.get_error());
        return 0;
    }
    const uint32_t color=window->decoration_argb;
    if (window->sdl.set_render_draw_color(window->renderer,(uint8_t)(color>>16),
            (uint8_t)(color>>8),(uint8_t)color,(uint8_t)(color>>24))!=0 ||
        window->sdl.render_clear(window->renderer)!=0) {
        set_error(error, error_capacity, "SDL decoration clear failed",
                  window->sdl.get_error());
        return 0;
    }
    const SdlRect destination={viewport.x,viewport.y,viewport.width,viewport.height};
    if (window->sdl.render_copy(window->renderer, window->texture, NULL,
                                &destination) !=
        0) {
        set_error(error, error_capacity, "SDL frame copy failed",
                  window->sdl.get_error());
        return 0;
    }

    window->sdl.render_present(window->renderer);
    return 1;
}

void ki_pc_window_delay(KiPcWindow *window, uint32_t milliseconds)
{
    if (window != NULL) {
        window->sdl.delay(milliseconds);
    }
}

void ki_pc_window_destroy(KiPcWindow *window)
{
    if (window == NULL) {
        return;
    }
    if (window->texture != NULL) {
        window->sdl.destroy_texture(window->texture);
    }
    if (window->renderer != NULL) {
        window->sdl.destroy_renderer(window->renderer);
    }
    if (window->window != NULL) {
        window->sdl.destroy_window(window->window);
    }
    if (window->library != NULL) {
        window->sdl.quit();
        dlclose(window->library);
    }
    free(window);
}
