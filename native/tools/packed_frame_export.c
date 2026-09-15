#include "ki/bgr555.h"
#include "ki/packed_frame.h"

/* Export recovered frames without depending on a windowing or image library. */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

enum { FRAME_HEADER_SIZE = 8 };

static void usage(const char *program)
{
    fprintf(stderr,
            "usage: %s <memory-dump> <frame-offset> <output> "
            "[scale [palette-offset]]\n",
            program);
    fprintf(stderr,
            "       without a palette writes PGM indices; with one writes "
            "a BGR555-derived PPM\n");
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

static uint16_t read_u16_le(const uint8_t *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8);
}

static uint8_t diagnostic_gray(uint8_t index)
{
    if (index == KI_PACKED_FRAME_TRANSPARENT) {
        return 0;
    }

    /* Preserve the five-bit ordering while keeping index zero visible. */
    return (uint8_t)(32u + ((unsigned int)index * 7u));
}

static int write_scaled_pgm(const char *path, const uint8_t *indices,
                            uint16_t width, uint16_t height,
                            unsigned int scale)
{
    FILE *output = fopen(path, "wb");
    if (output == NULL) {
        perror(path);
        return 0;
    }

    fprintf(output, "P5\n%u %u\n255\n", (unsigned int)width * scale,
            (unsigned int)height * scale);

    for (uint16_t y = 0; y < height; y++) {
        for (unsigned int repeat_y = 0; repeat_y < scale; repeat_y++) {
            for (uint16_t x = 0; x < width; x++) {
                const uint8_t gray =
                    diagnostic_gray(indices[(size_t)y * width + x]);
                for (unsigned int repeat_x = 0; repeat_x < scale; repeat_x++) {
                    if (fputc(gray, output) == EOF) {
                        perror(path);
                        fclose(output);
                        return 0;
                    }
                }
            }
        }
    }

    if (fclose(output) != 0) {
        perror(path);
        return 0;
    }
    return 1;
}

static uint8_t expand_5_to_8(uint16_t component)
{
    return (uint8_t)((component << 3) | (component >> 2));
}

static int write_scaled_ppm(const char *path, const uint8_t *indices,
                            uint16_t width, uint16_t height,
                            unsigned int scale, const uint8_t *palette)
{
    FILE *output = fopen(path, "wb");
    if (output == NULL) {
        perror(path);
        return 0;
    }

    fprintf(output, "P6\n%u %u\n255\n", (unsigned int)width * scale,
            (unsigned int)height * scale);

    for (uint16_t y = 0; y < height; y++) {
        for (unsigned int repeat_y = 0; repeat_y < scale; repeat_y++) {
            for (uint16_t x = 0; x < width; x++) {
                const uint8_t index = indices[(size_t)y * width + x];
                uint8_t rgb[3] = {0, 0, 0};

                if (index != KI_PACKED_FRAME_TRANSPARENT) {
                    /* On black, the original saturating blend yields the palette color. */
                    const uint16_t blend = read_u16_le(palette + index * 2u);
                    const uint16_t color =
                        ki_bgr555_saturating_add(0, blend);
                    rgb[0] = expand_5_to_8(color & 0x1fu);
                    rgb[1] = expand_5_to_8((color >> 5) & 0x1fu);
                    rgb[2] = expand_5_to_8((color >> 10) & 0x1fu);
                }

                for (unsigned int repeat_x = 0; repeat_x < scale; repeat_x++) {
                    if (fwrite(rgb, sizeof(rgb), 1, output) != 1) {
                        perror(path);
                        fclose(output);
                        return 0;
                    }
                }
            }
        }
    }

    if (fclose(output) != 0) {
        perror(path);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 4 || argc > 6) {
        usage(argv[0]);
        return 2;
    }

    char *offset_end = NULL;
    errno = 0;
    const uintmax_t parsed_offset = strtoumax(argv[2], &offset_end, 0);
    if (errno != 0 || offset_end == argv[2] || *offset_end != '\0' ||
        parsed_offset > SIZE_MAX) {
        fprintf(stderr, "%s: invalid frame offset\n", argv[2]);
        return 2;
    }

    unsigned int scale = 1;
    if (argc >= 5) {
        char *scale_end = NULL;
        errno = 0;
        const unsigned long parsed_scale = strtoul(argv[4], &scale_end, 0);
        if (errno != 0 || scale_end == argv[4] || *scale_end != '\0' ||
            parsed_scale == 0 || parsed_scale > 64) {
            fprintf(stderr, "%s: scale must be between 1 and 64\n", argv[4]);
            return 2;
        }
        scale = (unsigned int)parsed_scale;
    }

    size_t palette_offset = 0;
    const int use_palette = argc == 6;
    if (use_palette) {
        char *palette_end = NULL;
        errno = 0;
        const uintmax_t parsed_palette = strtoumax(argv[5], &palette_end, 0);
        if (errno != 0 || palette_end == argv[5] || *palette_end != '\0' ||
            parsed_palette > SIZE_MAX) {
            fprintf(stderr, "%s: invalid palette offset\n", argv[5]);
            return 2;
        }
        palette_offset = (size_t)parsed_palette;
    }

    uint8_t *dump = NULL;
    size_t dump_size = 0;
    if (!read_file(argv[1], &dump, &dump_size)) {
        return 1;
    }

    const size_t offset = (size_t)parsed_offset;
    if (offset > dump_size || FRAME_HEADER_SIZE > dump_size - offset) {
        fprintf(stderr, "frame offset 0x%zx is outside %s\n", offset, argv[1]);
        free(dump);
        return 1;
    }
    if (use_palette &&
        (palette_offset > dump_size || 64 > dump_size - palette_offset)) {
        fprintf(stderr, "palette offset 0x%zx is outside %s\n",
                palette_offset, argv[1]);
        free(dump);
        return 1;
    }

    const uint16_t width = read_u16_le(dump + offset + 4);
    const uint16_t height = read_u16_le(dump + offset + 6);
    if (width == 0 || height == 0 || width > SIZE_MAX / height) {
        fprintf(stderr, "frame at 0x%zx has invalid dimensions\n", offset);
        free(dump);
        return 1;
    }

    const size_t pixel_count = (size_t)width * height;
    uint8_t *indices = malloc(pixel_count);
    if (indices == NULL) {
        fprintf(stderr, "could not allocate %zu decoded pixels\n", pixel_count);
        free(dump);
        return 1;
    }

    KiPackedFrameInfo info = {0};
    const KiPackedFrameResult result =
        ki_decode_packed_frame(dump + offset, dump_size - offset, indices,
                               pixel_count, &info);
    if (result != KI_PACKED_FRAME_OK) {
        fprintf(stderr, "frame decode failed with result %d\n", result);
        free(indices);
        free(dump);
        return 1;
    }

    const int wrote_image = use_palette
                                ? write_scaled_ppm(
                                      argv[3], indices, info.width, info.height,
                                      scale, dump + palette_offset)
                                : write_scaled_pgm(argv[3], indices, info.width,
                                                   info.height, scale);
    if (wrote_image) {
        printf("decoded %ux%u frame at 0x%zx: origin=(%d,%d), "
               "packed_size=0x%zx, scale=%u, palette=%s\n",
               info.width, info.height, offset, info.origin_x, info.origin_y,
               info.bytes_consumed, scale,
               use_palette ? argv[5] : "diagnostic-indices");
    }

    free(indices);
    free(dump);
    return wrote_image ? 0 : 1;
}
