#include "ki/endokuken_capture.h"
#include "ki/packed_renderer.h"
#include "ki/pc_window.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SCREEN_WIDTH = 320,
    SCREEN_HEIGHT = 240,
    DEFAULT_WINDOW_SCALE = 3,
    PALETTE_OFFSET = 0x3480a,
    PALETTE_COLORS = 32
};

typedef struct DemoOptions {
    const char *memory_path;
    const char *background_path;
    const char *output_path;
    int background_raw;
    int raw_output;
    size_t selected_tick;
    unsigned int cycles;
    unsigned int window_scale;
    KiPcFitMode fit_mode;
    uint32_t decoration_argb;
    int fullscreen;
} DemoOptions;

static void print_usage(const char *program)
{
    printf("usage: %s [options]\n", program);
    puts("  --memory PATH      KI v1.5d main-RAM capture");
    puts("  --background PATH  optional 320x240 binary PPM scene");
    puts("  --background-raw PATH  optional little-endian BGR555 scene");
    puts("  --output PATH      render one tick to PPM instead of opening a window");
    puts("  --raw-output PATH  render one tick as little-endian BGR555 words");
    puts("  --tick N           animation tick used with --output (default: 0)");
    puts("  --cycles N         close the window after N animation cycles");
    puts("  --scale N          integer window scale from 1 to 8 (default: 3)");
    puts("  --fit MODE         integer (default) or aspect");
    puts("  --decor RRGGBB     host-only side-decoration color (default: 101018)");
    puts("  --fullscreen       start in desktop fullscreen (F11 toggles)");
}

static int parse_unsigned(const char *text,
                          unsigned int maximum,
                          unsigned int *result)
{
    char *end = NULL;
    errno = 0;
    const uintmax_t value = strtoumax(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || value > maximum) {
        return 0;
    }
    *result = (unsigned int)value;
    return 1;
}

static int parse_rgb(const char *text, uint32_t *result)
{
    char *end = NULL;
    errno = 0;
    const uintmax_t value = strtoumax(text, &end, 16);
    if (errno != 0 || strlen(text) != 6 || end != text + 6 ||
        value > UINT32_C(0xffffff)) return 0;
    *result = UINT32_C(0xff000000) | (uint32_t)value;
    return 1;
}

static int parse_options(int argc, char **argv, DemoOptions *options)
{
    *options = (DemoOptions){
        .memory_path = "work/mame/dumps/endokuken-mainram-27.1s.bin",
        .background_path = NULL,
        .output_path = NULL,
        .background_raw = 0,
        .raw_output = 0,
        .selected_tick = 0,
        .cycles = 0,
        .window_scale = DEFAULT_WINDOW_SCALE,
        .fit_mode = KI_PC_FIT_INTEGER,
        .decoration_argb = UINT32_C(0xff101018),
        .fullscreen = 0,
    };

    for (int argument = 1; argument < argc; argument++) {
        if (strcmp(argv[argument], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[argument], "--fullscreen") == 0) {
            options->fullscreen = 1;
            continue;
        }
        if (argument + 1 >= argc) {
            fprintf(stderr, "%s requires a value\n", argv[argument]);
            return -1;
        }

        const char *value = argv[++argument];
        if (strcmp(argv[argument - 1], "--memory") == 0) {
            options->memory_path = value;
        } else if (strcmp(argv[argument - 1], "--background") == 0) {
            options->background_path = value;
            options->background_raw = 0;
        } else if (strcmp(argv[argument - 1], "--background-raw") == 0) {
            options->background_path = value;
            options->background_raw = 1;
        } else if (strcmp(argv[argument - 1], "--output") == 0) {
            options->output_path = value;
            options->raw_output = 0;
        } else if (strcmp(argv[argument - 1], "--raw-output") == 0) {
            options->output_path = value;
            options->raw_output = 1;
        } else if (strcmp(argv[argument - 1], "--tick") == 0) {
            unsigned int tick = 0;
            if (!parse_unsigned(value, KI_ENDOKUKEN_CAPTURE_FRAME_COUNT - 1,
                                &tick)) {
                fprintf(stderr, "invalid animation tick: %s\n", value);
                return -1;
            }
            options->selected_tick = tick;
        } else if (strcmp(argv[argument - 1], "--cycles") == 0) {
            if (!parse_unsigned(value, 1000, &options->cycles)) {
                fprintf(stderr, "invalid cycle count: %s\n", value);
                return -1;
            }
        } else if (strcmp(argv[argument - 1], "--scale") == 0) {
            if (!parse_unsigned(value, 8, &options->window_scale) ||
                options->window_scale == 0) {
                fprintf(stderr, "window scale must be between 1 and 8\n");
                return -1;
            }
        } else if (strcmp(argv[argument - 1], "--fit") == 0) {
            if (strcmp(value, "integer") == 0)
                options->fit_mode = KI_PC_FIT_INTEGER;
            else if (strcmp(value, "aspect") == 0)
                options->fit_mode = KI_PC_FIT_ASPECT;
            else {
                fprintf(stderr, "fit mode must be integer or aspect\n");
                return -1;
            }
        } else if (strcmp(argv[argument - 1], "--decor") == 0) {
            if (!parse_rgb(value,&options->decoration_argb)) {
                fprintf(stderr,
                        "decoration color must be six hexadecimal digits\n");
                return -1;
            }
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[argument - 1]);
            return -1;
        }
    }
    return 1;
}

static int read_file(const char *path, uint8_t **contents, size_t *size)
{
    FILE *input = fopen(path, "rb");
    if (input == NULL) {
        perror(path);
        return 0;
    }
    if (fseek(input, 0, SEEK_END) != 0) {
        perror(path);
        fclose(input);
        return 0;
    }

    const long file_size = ftell(input);
    if (file_size < 0 || fseek(input, 0, SEEK_SET) != 0) {
        perror(path);
        fclose(input);
        return 0;
    }

    uint8_t *buffer = malloc((size_t)file_size);
    if (buffer == NULL) {
        fprintf(stderr, "%s: could not allocate %ld bytes\n", path, file_size);
        fclose(input);
        return 0;
    }
    if (fread(buffer, 1, (size_t)file_size, input) != (size_t)file_size) {
        perror(path);
        free(buffer);
        fclose(input);
        return 0;
    }

    fclose(input);
    *contents = buffer;
    *size = (size_t)file_size;
    return 1;
}

static int read_ppm_token(FILE *input, char *token, size_t capacity)
{
    int character;
    do {
        character = fgetc(input);
        if (character == '#') {
            do {
                character = fgetc(input);
            } while (character != '\n' && character != EOF);
        }
    } while (character == ' ' || character == '\t' || character == '\r' ||
             character == '\n');

    if (character == EOF) {
        return 0;
    }

    size_t length = 0;
    do {
        if (length + 1 >= capacity) {
            return 0;
        }
        token[length++] = (char)character;
        character = fgetc(input);
    } while (character != EOF && character != ' ' && character != '\t' &&
             character != '\r' && character != '\n');
    token[length] = '\0';
    return 1;
}

static uint16_t rgb8_to_bgr555(uint8_t red, uint8_t green, uint8_t blue)
{
    const uint16_t red5 = (uint16_t)(((unsigned int)red * 31u + 127u) / 255u);
    const uint16_t green5 =
        (uint16_t)(((unsigned int)green * 31u + 127u) / 255u);
    const uint16_t blue5 =
        (uint16_t)(((unsigned int)blue * 31u + 127u) / 255u);
    return (uint16_t)(red5 | (green5 << 5) | (blue5 << 10));
}

static int load_background_ppm(const char *path, uint16_t *pixels)
{
    FILE *input = fopen(path, "rb");
    if (input == NULL) {
        perror(path);
        return 0;
    }

    char token[32];
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int maximum = 0;
    if (!read_ppm_token(input, token, sizeof(token)) || strcmp(token, "P6") != 0 ||
        !read_ppm_token(input, token, sizeof(token)) ||
        !parse_unsigned(token, 65535, &width) ||
        !read_ppm_token(input, token, sizeof(token)) ||
        !parse_unsigned(token, 65535, &height) ||
        !read_ppm_token(input, token, sizeof(token)) ||
        !parse_unsigned(token, 65535, &maximum) || width != SCREEN_WIDTH ||
        height != SCREEN_HEIGHT || maximum != 255) {
        fprintf(stderr, "%s: expected a 320x240 binary PPM with max value 255\n",
                path);
        fclose(input);
        return 0;
    }

    for (size_t pixel = 0; pixel < SCREEN_WIDTH * SCREEN_HEIGHT; pixel++) {
        uint8_t rgb[3];
        if (fread(rgb, sizeof(rgb), 1, input) != 1) {
            fprintf(stderr, "%s: truncated pixel data\n", path);
            fclose(input);
            return 0;
        }
        pixels[pixel] = rgb8_to_bgr555(rgb[0], rgb[1], rgb[2]);
    }

    fclose(input);
    return 1;
}

static int load_background_bgr555(const char *path, uint16_t *pixels)
{
    uint8_t *bytes = NULL;
    size_t size = 0;
    const size_t expected_size =
        SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(*pixels);

    if (!read_file(path, &bytes, &size)) {
        return 0;
    }
    if (size != expected_size) {
        fprintf(stderr, "%s: expected exactly %zu bytes of BGR555 pixels\n",
                path, expected_size);
        free(bytes);
        return 0;
    }

    /* MAME's R4600 and the native capture both store each word little-endian. */
    for (size_t pixel = 0; pixel < SCREEN_WIDTH * SCREEN_HEIGHT; pixel++) {
        pixels[pixel] = (uint16_t)bytes[pixel * 2u] |
                        ((uint16_t)bytes[pixel * 2u + 1u] << 8);
    }
    free(bytes);
    return 1;
}

static void make_diagnostic_background(uint16_t *pixels)
{
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            const uint16_t red = (uint16_t)((x * 10 / SCREEN_WIDTH) + 2);
            const uint16_t green = (uint16_t)((y * 8 / SCREEN_HEIGHT) + 2);
            const uint16_t blue = (uint16_t)(y < 72 ? 8 : 3);
            uint16_t color = (uint16_t)(red | (green << 5) | (blue << 10));

            /* A quiet grid makes placement and upward row orientation obvious. */
            if (x % 32 == 0 || y % 24 == 0) {
                color = (uint16_t)(color + 0x0421);
            }
            pixels[(size_t)y * SCREEN_WIDTH + x] = color;
        }
    }
}

static uint16_t read_u16_le(const uint8_t *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8);
}

static uint8_t expand_5_to_8(uint16_t component)
{
    return (uint8_t)((component << 3) | (component >> 2));
}

static void convert_to_argb(const uint16_t *source, uint32_t *destination)
{
    for (size_t pixel = 0; pixel < SCREEN_WIDTH * SCREEN_HEIGHT; pixel++) {
        const uint16_t color = source[pixel];
        const uint32_t red = expand_5_to_8(color & 0x1fu);
        const uint32_t green = expand_5_to_8((color >> 5) & 0x1fu);
        const uint32_t blue = expand_5_to_8((color >> 10) & 0x1fu);
        destination[pixel] =
            0xff000000u | (red << 16) | (green << 8) | blue;
    }
}

static int write_ppm(const char *path, const uint16_t *pixels)
{
    FILE *output = fopen(path, "wb");
    if (output == NULL) {
        perror(path);
        return 0;
    }
    fprintf(output, "P6\n%d %d\n255\n", SCREEN_WIDTH, SCREEN_HEIGHT);

    for (size_t pixel = 0; pixel < SCREEN_WIDTH * SCREEN_HEIGHT; pixel++) {
        const uint16_t color = pixels[pixel];
        const uint8_t rgb[3] = {
            expand_5_to_8(color & 0x1fu),
            expand_5_to_8((color >> 5) & 0x1fu),
            expand_5_to_8((color >> 10) & 0x1fu),
        };
        if (fwrite(rgb, sizeof(rgb), 1, output) != 1) {
            perror(path);
            fclose(output);
            return 0;
        }
    }
    return fclose(output) == 0;
}

static int write_raw_bgr555(const char *path, const uint16_t *pixels)
{
    FILE *output = fopen(path, "wb");
    if (output == NULL) {
        perror(path);
        return 0;
    }

    for (size_t pixel = 0; pixel < SCREEN_WIDTH * SCREEN_HEIGHT; pixel++) {
        const uint8_t little_endian[2] = {
            (uint8_t)(pixels[pixel] & 0xffu),
            (uint8_t)(pixels[pixel] >> 8),
        };
        if (fwrite(little_endian, sizeof(little_endian), 1, output) != 1) {
            perror(path);
            fclose(output);
            return 0;
        }
    }
    return fclose(output) == 0;
}

static int render_capture_frame(size_t tick,
                                const uint8_t *memory,
                                size_t memory_size,
                                const uint16_t palette[PALETTE_COLORS],
                                const uint16_t *background,
                                uint16_t *scene,
                                KiPackedRenderStats *stats)
{
    memcpy(scene, background,
           SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(*scene));
    KiBgr555Surface surface = {scene, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH};
    const KiEndokukenCaptureFrame *frame =
        &KI_ENDOKUKEN_CAPTURE_FRAMES[tick];

    *stats = (KiPackedRenderStats){0};
    for (size_t index = 0; index < frame->sample_count; index++) {
        const KiEndokukenCaptureSample *sample =
            &KI_ENDOKUKEN_CAPTURE_SAMPLES[frame->first_sample + index];
        if (sample->frame_offset >= memory_size) {
            fprintf(stderr, "frame offset 0x%zx is outside the RAM capture\n",
                    sample->frame_offset);
            return 0;
        }

        KiPackedRenderStats particle_stats = {0};
        const KiPackedRenderResult result = ki_render_packed_frame_bgr555(
            memory + sample->frame_offset, memory_size - sample->frame_offset,
            palette, &surface, &sample->transform, &particle_stats);
        if (result != KI_PACKED_RENDER_OK) {
            fprintf(stderr,
                    "tick %zu object 0x%08" PRIx32
                    " token 0x%02x render failed with result %d\n",
                    tick, sample->object_address, sample->token, result);
            return 0;
        }

        /* Saturating blends are order-dependent, so the captured pass order
         * is retained and its counters are accumulated only for diagnostics. */
        stats->output_rows += particle_stats.output_rows;
        stats->source_rows += particle_stats.source_rows;
        stats->blended_pixels += particle_stats.blended_pixels;
        stats->clipped_pixels += particle_stats.clipped_pixels;
    }
    return 1;
}

int main(int argc, char **argv)
{
    DemoOptions options;
    const int parsed = parse_options(argc, argv, &options);
    if (parsed <= 0) {
        return parsed == 0 ? 0 : 2;
    }

    uint8_t *memory = NULL;
    size_t memory_size = 0;
    if (!read_file(options.memory_path, &memory, &memory_size)) {
        return 1;
    }
    if (PALETTE_OFFSET + PALETTE_COLORS * 2u > memory_size) {
        fprintf(stderr, "%s does not contain the recovered palette\n",
                options.memory_path);
        free(memory);
        return 1;
    }

    uint16_t palette[PALETTE_COLORS];
    for (size_t index = 0; index < PALETTE_COLORS; index++) {
        palette[index] = read_u16_le(memory + PALETTE_OFFSET + index * 2u);
    }

    uint16_t *background =
        malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(*background));
    uint16_t *scene = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(*scene));
    uint32_t *argb = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(*argb));
    if (background == NULL || scene == NULL || argb == NULL) {
        fputs("could not allocate the native scene buffers\n", stderr);
        free(argb);
        free(scene);
        free(background);
        free(memory);
        return 1;
    }

    if (options.background_path != NULL) {
        const int loaded = options.background_raw
                               ? load_background_bgr555(options.background_path,
                                                        background)
                               : load_background_ppm(options.background_path,
                                                     background);
        if (!loaded) {
            free(argb);
            free(scene);
            free(background);
            free(memory);
            return 1;
        }
    } else {
        make_diagnostic_background(background);
    }

    if (options.output_path != NULL) {
        KiPackedRenderStats stats = {0};
        if (!render_capture_frame(options.selected_tick, memory, memory_size,
                                  palette, background, scene, &stats) ||
            !(options.raw_output
                  ? write_raw_bgr555(options.output_path, scene)
                  : write_ppm(options.output_path, scene))) {
            free(argb);
            free(scene);
            free(background);
            free(memory);
            return 1;
        }
        printf("rendered tick %zu (%u particles): %u rows, "
               "%u blended pixels, %u clipped pixels -> %s\n",
               options.selected_tick,
               KI_ENDOKUKEN_CAPTURE_FRAMES[options.selected_tick].sample_count,
               stats.output_rows, stats.blended_pixels, stats.clipped_pixels,
               options.output_path);
        free(argb);
        free(scene);
        free(background);
        free(memory);
        return 0;
    }

    char window_error[256] = {0};
    const KiPcWindowOptions window_options={SCREEN_WIDTH,SCREEN_HEIGHT,
        (int)options.window_scale,options.fit_mode,options.decoration_argb,
        options.fullscreen};
    KiPcWindow *window = ki_pc_window_create_with_options(
        "Meltdown - capture-backed diagnostic", &window_options,
        window_error, sizeof(window_error));
    if (window == NULL) {
        fprintf(stderr, "%s\n", window_error);
        free(argb);
        free(scene);
        free(background);
        free(memory);
        return 1;
    }

    const size_t sample_count = KI_ENDOKUKEN_CAPTURE_FRAME_COUNT;
    size_t tick = 0;
    size_t presented_frames = 0;
    int running = 1;
    while (running && ki_pc_window_process_events(window)) {
        KiPackedRenderStats stats = {0};
        if (!render_capture_frame(tick, memory, memory_size, palette, background,
                                  scene, &stats)) {
            running = 0;
            break;
        }
        convert_to_argb(scene, argb);
        if (!ki_pc_window_present(window, argb, SCREEN_WIDTH, window_error,
                                  sizeof(window_error))) {
            fprintf(stderr, "%s\n", window_error);
            running = 0;
            break;
        }

        tick = (tick + 1) % sample_count;
        presented_frames++;
        if (options.cycles != 0 &&
            presented_frames >= sample_count * options.cycles) {
            break;
        }

        /* 17, 17, 16 ms averages 60 Hz without requiring a timer subsystem. */
        ki_pc_window_delay(window, tick % 3 == 2 ? 16u : 17u);
    }

    ki_pc_window_destroy(window);
    free(argb);
    free(scene);
    free(background);
    free(memory);
    return running ? 0 : 1;
}
