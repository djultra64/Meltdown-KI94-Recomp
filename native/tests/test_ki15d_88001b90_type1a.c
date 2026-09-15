#include "ki/original/ki15d_88001b90_type1a.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAIN_RAM_SIZE = 1024 * 1024,
    MAX_FIXTURE_CASES = 315,
    FIXTURE_FIELD_COUNT = 44,
    LINE_CAPACITY = 1024,
    SCREEN_WIDTH = 320,
    SCREEN_HEIGHT = 240
};

static const uint32_t FRAMEBUFFER_SELECT_ADDRESS = UINT32_C(0x88086200);
static const uint32_t VIEWPORT_HEIGHT_ADDRESS = UINT32_C(0x88087b40);

typedef struct FixtureCase {
    uint32_t call_id;
    uint32_t tick;
    uint32_t pass_index;
    char milestones[96];
    uint32_t record_address;
    uint8_t token;
    int8_t pixel_step;
    uint32_t frame_address;
    uint16_t record_scale_x;
    uint16_t record_scale_y;
    uint32_t word68;
    uint32_t word6c;
    uint32_t word70;
    int8_t facing;
    uint8_t display_class;
    uint8_t flags;
    uint16_t horizontal_minimum;
    uint16_t horizontal_limit;
    uint8_t framebuffer_select;
    uint32_t viewport_height;
    int16_t frame_origin_x;
    int16_t frame_origin_y;
    uint16_t frame_width;
    uint16_t frame_height;
    uint16_t skipped_source_rows;
    uint16_t skipped_source_bytes;
    uint32_t palette_source;
    uint32_t framebuffer_address;
    uint16_t scaled_width;
    Ki15dHorizontalClip horizontal_clip;
    uint8_t render_mode;
    uint32_t row_callback;
    KiPackedRenderTransform transform;
    uint8_t field8e;
    uint8_t field97;
} FixtureCase;

typedef enum DeveloperRenderSource {
    DEVELOPER_RENDER_COMPARE = 0,
    DEVELOPER_RENDER_ORACLE = 1,
    DEVELOPER_RENDER_STATE = 2
} DeveloperRenderSource;

static int split_fields(char *line, char **fields)
{
    size_t count = 0;
    char *start = line;
    for (char *cursor = line;; cursor++) {
        if (*cursor == ',' || *cursor == '\n' || *cursor == '\r' ||
            *cursor == '\0') {
            const char terminator = *cursor;
            *cursor = '\0';
            if (count >= FIXTURE_FIELD_COUNT) {
                return 0;
            }
            fields[count++] = start;
            if (terminator != ',') {
                break;
            }
            start = cursor + 1;
        }
    }
    return count == FIXTURE_FIELD_COUNT;
}

static int parse_u32(const char *text, int base, uint32_t *value)
{
    char *end = NULL;
    errno = 0;
    const uintmax_t parsed = strtoumax(text, &end, base);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static int parse_i32(const char *text, int32_t *value)
{
    char *end = NULL;
    errno = 0;
    const intmax_t parsed = strtoimax(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < INT32_MIN ||
        parsed > INT32_MAX) {
        return 0;
    }
    *value = (int32_t)parsed;
    return 1;
}

static int narrow_u16(uint32_t value, uint16_t *result)
{
    if (value > UINT16_MAX) {
        return 0;
    }
    *result = (uint16_t)value;
    return 1;
}

static int narrow_u8(uint32_t value, uint8_t *result)
{
    if (value > UINT8_MAX) {
        return 0;
    }
    *result = (uint8_t)value;
    return 1;
}

static int narrow_i16(int32_t value, int16_t *result)
{
    if (value < INT16_MIN || value > INT16_MAX) {
        return 0;
    }
    *result = (int16_t)value;
    return 1;
}

static int narrow_i8(int32_t value, int8_t *result)
{
    if (value < INT8_MIN || value > INT8_MAX) {
        return 0;
    }
    *result = (int8_t)value;
    return 1;
}

static int parse_clip(const char *text, Ki15dHorizontalClip *clip)
{
    if (strcmp(text, "none") == 0) {
        *clip = KI15D_HORIZONTAL_CLIP_NONE;
    } else if (strcmp(text, "left") == 0) {
        *clip = KI15D_HORIZONTAL_CLIP_LEFT;
    } else if (strcmp(text, "right") == 0) {
        *clip = KI15D_HORIZONTAL_CLIP_RIGHT;
    } else if (strcmp(text, "both") == 0) {
        *clip = KI15D_HORIZONTAL_CLIP_BOTH;
    } else {
        return 0;
    }
    return 1;
}

static int parse_fixture_case(char **field, FixtureCase *fixture)
{
    uint32_t value = 0;
    int32_t signed_value = 0;
    memset(fixture, 0, sizeof(*fixture));

#define U32(index, base, destination)                                      \
    (parse_u32(field[index], base, &(destination)))
#define I32(index, destination) (parse_i32(field[index], &(destination)))
#define U16(index, base, destination)                                      \
    (parse_u32(field[index], base, &value) && narrow_u16(value, &(destination)))
#define U8(index, base, destination)                                       \
    (parse_u32(field[index], base, &value) && narrow_u8(value, &(destination)))

    if (!U32(0, 10, fixture->call_id) || !U32(1, 10, fixture->tick) ||
        !U32(2, 10, fixture->pass_index) || strlen(field[3]) >=
            sizeof(fixture->milestones) ||
        !U32(4, 16, fixture->record_address) ||
        !U8(5, 16, fixture->token) || !I32(6, signed_value) ||
        !narrow_i8(signed_value, &fixture->pixel_step) ||
        !U32(7, 16, fixture->frame_address) ||
        !U16(8, 16, fixture->record_scale_x) ||
        !U16(9, 16, fixture->record_scale_y) ||
        !U32(10, 16, fixture->word68) || !U32(11, 16, fixture->word6c) ||
        !U32(12, 16, fixture->word70) || !I32(13, signed_value) ||
        !narrow_i8(signed_value, &fixture->facing) ||
        !U8(14, 16, fixture->display_class) ||
        !U8(15, 16, fixture->flags) ||
        !U16(16, 10, fixture->horizontal_minimum) ||
        !U16(17, 10, fixture->horizontal_limit) ||
        !U8(18, 10, fixture->framebuffer_select) ||
        !U32(19, 10, fixture->viewport_height) ||
        !I32(20, signed_value) ||
        !narrow_i16(signed_value, &fixture->frame_origin_x) ||
        !I32(21, signed_value) ||
        !narrow_i16(signed_value, &fixture->frame_origin_y) ||
        !U16(22, 10, fixture->frame_width) ||
        !U16(23, 10, fixture->frame_height) ||
        !U16(24, 10, fixture->skipped_source_rows) ||
        !U16(25, 10, fixture->skipped_source_bytes) ||
        !U32(26, 16, fixture->palette_source) ||
        !U32(27, 16, fixture->framebuffer_address) ||
        !U16(28, 10, fixture->scaled_width) ||
        !parse_clip(field[29], &fixture->horizontal_clip) ||
        !U8(30, 16, fixture->render_mode) ||
        !U32(31, 16, fixture->row_callback) ||
        !I32(32, fixture->transform.destination_x) ||
        !I32(33, fixture->transform.destination_y) ||
        !I32(34, fixture->transform.x_direction) ||
        !I32(35, fixture->transform.y_direction) ||
        !U32(36, 16, fixture->transform.x_scale) ||
        !U32(37, 16, fixture->transform.x_remainder) ||
        !U32(38, 16, fixture->transform.y_accumulator) ||
        !U32(39, 16, fixture->transform.y_scale) ||
        !U32(40, 16, fixture->transform.source_offset) ||
        !U32(41, 10, fixture->transform.output_rows) ||
        !U8(42, 16, fixture->field8e) || !U8(43, 16, fixture->field97)) {
        return 0;
    }
    memcpy(fixture->milestones, field[3], strlen(field[3]) + 1u);
    return 1;

#undef U32
#undef I32
#undef U16
#undef U8
}

static KiMemory test_memory(uint8_t *ram, size_t size)
{
    const KiMemory memory = {
        .low_ram = NULL,
        .low_ram_size = 0,
        .main_ram = ram,
        .main_ram_size = size,
        .boot_rom = NULL,
        .boot_rom_size = 0,
    };
    return memory;
}

static int initialize_guest_state(KiMemory *memory, const FixtureCase *fixture)
{
    const uint32_t record = fixture->record_address;
    const uint32_t frame = fixture->frame_address;
    if (ki_memory_write_u8(memory, record, 0x1a) != KI_MEMORY_OK ||
        ki_memory_write_u8(memory, record + 0x14u, fixture->token) !=
            KI_MEMORY_OK ||
        ki_memory_write_u8(memory, record + 0x24u,
                           (uint8_t)fixture->pixel_step) != KI_MEMORY_OK ||
        ki_memory_write_u32_le(memory, record + 0x30u, frame) != KI_MEMORY_OK ||
        ki_memory_write_u16_le(memory, record + 0x58u,
                               fixture->record_scale_x) != KI_MEMORY_OK ||
        ki_memory_write_u16_le(memory, record + 0x5au,
                               fixture->record_scale_y) != KI_MEMORY_OK ||
        ki_memory_write_u32_le(memory, record + 0x68u, fixture->word68) !=
            KI_MEMORY_OK ||
        ki_memory_write_u32_le(memory, record + 0x6cu, fixture->word6c) !=
            KI_MEMORY_OK ||
        ki_memory_write_u32_le(memory, record + 0x70u, fixture->word70) !=
            KI_MEMORY_OK ||
        ki_memory_write_u8(memory, record + 0x8cu,
                           (uint8_t)fixture->facing) != KI_MEMORY_OK ||
        ki_memory_write_u8(memory, record + 0x8eu, fixture->field8e) !=
            KI_MEMORY_OK ||
        ki_memory_write_u8(memory, record + 0x94u,
                           fixture->display_class) != KI_MEMORY_OK ||
        ki_memory_write_u8(memory, record + 0x95u, fixture->flags) !=
            KI_MEMORY_OK ||
        ki_memory_write_u8(memory, record + 0x97u, fixture->field97) !=
            KI_MEMORY_OK ||
        ki_memory_write_u16_le(memory, record + 0xc8u,
                               fixture->horizontal_minimum) != KI_MEMORY_OK ||
        ki_memory_write_u16_le(memory, record + 0xcau,
                               fixture->horizontal_limit) != KI_MEMORY_OK ||
        ki_memory_write_u8(memory, FRAMEBUFFER_SELECT_ADDRESS,
                           fixture->framebuffer_select) != KI_MEMORY_OK ||
        ki_memory_write_u32_le(memory, VIEWPORT_HEIGHT_ADDRESS,
                               fixture->viewport_height) != KI_MEMORY_OK ||
        ki_memory_write_u16_le(memory, frame,
                               (uint16_t)fixture->frame_origin_x) !=
            KI_MEMORY_OK ||
        ki_memory_write_u16_le(memory, frame + 2u,
                               (uint16_t)fixture->frame_origin_y) !=
            KI_MEMORY_OK ||
        ki_memory_write_u16_le(memory, frame + 4u, fixture->frame_width) !=
            KI_MEMORY_OK ||
        ki_memory_write_u16_le(memory, frame + 6u, fixture->frame_height) !=
            KI_MEMORY_OK) {
        return 0;
    }

    if (fixture->skipped_source_rows > 1 ||
        (fixture->skipped_source_rows != 0 &&
         fixture->skipped_source_bytes == 0)) {
        return 0;
    }

    uint32_t row_offset = 8;
    for (uint16_t row = 0; row < fixture->frame_height; row++) {
        const uint32_t row_size =
            row == 0 && fixture->skipped_source_rows != 0
                ? fixture->skipped_source_bytes
                : 3u;
        if (row_size > UINT8_MAX || UINT32_MAX - row_offset < row_size ||
            ki_memory_write_u8(memory, frame + row_offset,
                               (uint8_t)row_size) != KI_MEMORY_OK) {
            return 0;
        }
        row_offset += row_size;
    }
    return 1;
}

static int compare_call(const FixtureCase *expected,
                        const Ki15dType1aRenderCall *actual)
{
#define CHECK(field, expected_value, actual_value)                           \
    do {                                                                     \
        if ((expected_value) != (actual_value)) {                            \
            fprintf(stderr, "call %" PRIu32 ": %s expected=%" PRId64       \
                            " actual=%" PRId64 "\n",                        \
                    expected->call_id, field, (int64_t)(expected_value),     \
                    (int64_t)(actual_value));                                \
            return 0;                                                        \
        }                                                                    \
    } while (0)

    CHECK("frame_address", expected->frame_address, actual->frame_address);
    CHECK("palette_source", expected->palette_source,
          actual->palette_source_address);
    CHECK("palette_table", UINT32_C(0x8803480a),
          actual->palette_table_address);
    CHECK("framebuffer", expected->framebuffer_address,
          actual->framebuffer_address);
    CHECK("row_callback", expected->row_callback,
          actual->row_callback_address);
    CHECK("frame_width", expected->frame_width, actual->frame_width);
    CHECK("frame_height", expected->frame_height, actual->frame_height);
    CHECK("scaled_width", expected->scaled_width, actual->scaled_width);
    CHECK("framebuffer_bank", expected->framebuffer_select & 1u,
          actual->framebuffer_bank);
    CHECK("render_mode", expected->render_mode, actual->render_mode);
    CHECK("horizontal_clip", expected->horizontal_clip,
          actual->horizontal_clip);
    CHECK("destination_x", expected->transform.destination_x,
          actual->transform.destination_x);
    CHECK("destination_y", expected->transform.destination_y,
          actual->transform.destination_y);
    CHECK("x_direction", expected->transform.x_direction,
          actual->transform.x_direction);
    CHECK("y_direction", expected->transform.y_direction,
          actual->transform.y_direction);
    CHECK("x_scale", expected->transform.x_scale, actual->transform.x_scale);
    CHECK("x_remainder", expected->transform.x_remainder,
          actual->transform.x_remainder);
    CHECK("y_accumulator", expected->transform.y_accumulator,
          actual->transform.y_accumulator);
    CHECK("y_scale", expected->transform.y_scale, actual->transform.y_scale);
    CHECK("source_offset", expected->transform.source_offset,
          actual->transform.source_offset);
    CHECK("output_rows", expected->transform.output_rows,
          actual->transform.output_rows);
    return 1;

#undef CHECK
}

static uint8_t *make_synthetic_frame(const FixtureCase *fixture, size_t *size)
{
    const size_t capacity = 8u + (size_t)fixture->frame_height * 5u;
    uint8_t *frame = calloc(capacity, 1);
    if (frame == NULL) {
        return NULL;
    }
    frame[0] = (uint8_t)fixture->frame_origin_x;
    frame[1] = (uint8_t)((uint16_t)fixture->frame_origin_x >> 8);
    frame[2] = (uint8_t)fixture->frame_origin_y;
    frame[3] = (uint8_t)((uint16_t)fixture->frame_origin_y >> 8);
    frame[4] = (uint8_t)fixture->frame_width;
    frame[5] = (uint8_t)(fixture->frame_width >> 8);
    frame[6] = (uint8_t)fixture->frame_height;
    frame[7] = (uint8_t)(fixture->frame_height >> 8);

    size_t offset = 8;
    for (uint16_t row = 0; row < fixture->frame_height; row++) {
        const size_t row_size =
            row == 0 && fixture->skipped_source_rows != 0
                ? fixture->skipped_source_bytes
                : 3u;
        frame[offset] = (uint8_t)row_size;
        if (!(row == 0 && fixture->skipped_source_rows != 0)) {
            frame[offset + 1u] = 0;
            frame[offset + 2u] = 0x08; /* one pixel using palette index 1 */
        }
        offset += row_size;
    }
    *size = offset;
    return frame;
}

static int render_one(const FixtureCase *fixture,
                      const KiPackedRenderTransform *transform,
                      uint16_t *scene)
{
    size_t frame_size = 0;
    uint8_t *frame = make_synthetic_frame(fixture, &frame_size);
    if (frame == NULL) {
        return 0;
    }
    uint16_t palette[32] = {0};
    palette[1] = 0x0421;
    KiBgr555Surface surface = {
        scene, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH};
    KiPackedRenderStats stats = {0};
    const KiPackedRenderResult result = ki_render_packed_frame_bgr555(
        frame, frame_size, palette, &surface, transform, &stats);
    free(frame);
    return result == KI_PACKED_RENDER_OK;
}

static int verify_ordered_render_oracle(
    const FixtureCase *fixtures,
    const Ki15dType1aRenderCall *actual,
    size_t count,
    DeveloperRenderSource source,
    uint64_t *hash)
{
    uint32_t peak_tick = UINT32_MAX;
    for (size_t index = 0; index < count; index++) {
        if (strstr(fixtures[index].milestones, "six_particle_peak") != NULL) {
            peak_tick = fixtures[index].tick;
            break;
        }
    }
    if (peak_tick == UINT32_MAX) {
        fputs("fixture has no six-particle peak\n", stderr);
        return 0;
    }

    uint16_t *expected_scene = calloc(SCREEN_WIDTH * SCREEN_HEIGHT,
                                      sizeof(*expected_scene));
    uint16_t *actual_scene = calloc(SCREEN_WIDTH * SCREEN_HEIGHT,
                                    sizeof(*actual_scene));
    if (expected_scene == NULL || actual_scene == NULL) {
        free(actual_scene);
        free(expected_scene);
        return 0;
    }

    unsigned int peak_calls = 0;
    uint64_t value = UINT64_C(1469598103934665603);
    for (size_t index = 0; index < count; index++) {
        const int endpoint = index == 0 || index + 1u == count;
        const int at_peak = fixtures[index].tick == peak_tick;
        if (!endpoint && !at_peak) {
            continue;
        }
        if (at_peak && peak_calls == 0) {
            memset(expected_scene, 0,
                   SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(*expected_scene));
            memset(actual_scene, 0,
                   SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(*actual_scene));
        }
        if (at_peak) {
            peak_calls++;
        } else {
            memset(expected_scene, 0,
                   SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(*expected_scene));
            memset(actual_scene, 0,
                   SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(*actual_scene));
        }
        const int oracle_ok =
            source == DEVELOPER_RENDER_STATE ||
            render_one(&fixtures[index], &fixtures[index].transform,
                       expected_scene);
        const int state_ok =
            source == DEVELOPER_RENDER_ORACLE ||
            render_one(&fixtures[index], &actual[index].transform,
                       actual_scene);
        const int comparison_ok =
            source != DEVELOPER_RENDER_COMPARE ||
            memcmp(expected_scene, actual_scene,
                   SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(*actual_scene)) == 0;
        if (!oracle_ok || !state_ok || !comparison_ok) {
            fprintf(stderr,
                    "call %" PRIu32 ": ordered renderer output diverged\n",
                    fixtures[index].call_id);
            free(actual_scene);
            free(expected_scene);
            return 0;
        }
        const uint16_t *hashed_scene =
            source == DEVELOPER_RENDER_ORACLE ? expected_scene : actual_scene;
        for (size_t pixel = 0; pixel < SCREEN_WIDTH * SCREEN_HEIGHT; pixel++) {
            value ^= hashed_scene[pixel];
            value *= UINT64_C(1099511628211);
        }
    }
    free(actual_scene);
    free(expected_scene);
    if (peak_calls != 6) {
        fprintf(stderr, "ordered oracle expected 6 peak calls, got %u\n",
                peak_calls);
        return 0;
    }

    *hash = value;
    return 1;
}

static FixtureCase synthetic_case(void)
{
    FixtureCase fixture = {0};
    fixture.record_address = 0x8808c000;
    fixture.pixel_step = 2;
    fixture.frame_address = 0x88097670;
    fixture.word68 = 50u << 16;
    fixture.word6c = 100u << 16;
    fixture.word70 = 0x1000;
    fixture.display_class = 0x60;
    fixture.horizontal_minimum = 10;
    fixture.horizontal_limit = 100;
    fixture.viewport_height = 240;
    fixture.frame_width = 20;
    fixture.frame_height = 8;
    return fixture;
}

static int prepare_synthetic(uint8_t *ram,
                             const FixtureCase *fixture,
                             Ki15dType1aRenderCall *call,
                             Ki15dType1aRenderResult *result)
{
    memset(ram, 0, MAIN_RAM_SIZE);
    KiMemory memory = test_memory(ram, MAIN_RAM_SIZE);
    if (!initialize_guest_state(&memory, fixture)) {
        return 0;
    }
    *result = ki15d_88001b90_type1a_prepare_render(
        &memory, fixture->record_address, call);
    return 1;
}

static int verify_boundaries(uint8_t *ram)
{
    Ki15dType1aRenderCall call = {0};
    Ki15dType1aRenderResult result = KI15D_TYPE1A_RENDER_INVALID_ARGUMENT;
    FixtureCase fixture = synthetic_case();

    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_OK ||
        call.horizontal_clip != KI15D_HORIZONTAL_CLIP_NONE ||
        call.render_mode != 0x60 ||
        call.row_callback_address != UINT32_C(0x88011918) ||
        call.transform.x_scale != 0x1000) {
        fputs("unscaled/unclipped boundary case failed\n", stderr);
        return 0;
    }

    fixture.horizontal_limit = 0;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_OK ||
        call.horizontal_clip != KI15D_HORIZONTAL_CLIP_NONE) {
        fputs("zero-width default-to-320 case failed\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.word68 = 5u << 16;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_OK ||
        call.horizontal_clip != KI15D_HORIZONTAL_CLIP_LEFT ||
        call.render_mode != 0x68 ||
        call.row_callback_address != UINT32_C(0x880118c0)) {
        fputs("left clip boundary case failed\n", stderr);
        return 0;
    }

    fixture.word68 = 5u << 16;
    fixture.frame_width = 100;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_OK ||
        call.horizontal_clip != KI15D_HORIZONTAL_CLIP_BOTH) {
        fputs("both-edge clip boundary case failed\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.pixel_step = -2;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_OK || call.transform.x_direction != -1 ||
        call.horizontal_clip != KI15D_HORIZONTAL_CLIP_NONE ||
        call.render_mode != 0x64 ||
        call.row_callback_address != UINT32_C(0x88011a24)) {
        fputs("reverse direction boundary case failed\n", stderr);
        return 0;
    }

    fixture.word68 = 105u << 16;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_OK ||
        call.horizontal_clip != KI15D_HORIZONTAL_CLIP_RIGHT ||
        call.render_mode != 0x6c ||
        call.row_callback_address != UINT32_C(0x880119c4)) {
        fputs("reverse right clip boundary case failed\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.word68 = (uint32_t)(int32_t)(-30 * 65536);
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_NOT_VISIBLE) {
        fputs("fully horizontal-offscreen case failed\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.word6c = (uint32_t)(int32_t)(-10 * 65536);
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_NOT_VISIBLE) {
        fputs("below-top vertical case failed\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.word6c = 300u << 16;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_NOT_VISIBLE) {
        fputs("wholly-beyond high-edge case failed\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.word6c = 245u << 16;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_OK ||
        call.transform.destination_y != 239 ||
        call.transform.source_offset != 26 ||
        call.transform.output_rows != 2 ||
        call.transform.y_accumulator != 0x17ff) {
        fputs("partial high-edge source walk case failed\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.word6c = 240u << 16;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_OK ||
        call.transform.destination_y != 239 ||
        call.transform.source_offset != 11 ||
        call.transform.output_rows != 7) {
        fputs("exact high-edge source walk case failed\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.word6c = 241u << 16;
    fixture.word70 = 0x0800;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_OK ||
        call.transform.destination_y != 239 ||
        call.transform.source_offset != 23 ||
        call.transform.output_rows != 2 ||
        call.transform.y_accumulator != 0x17ff) {
        fputs("combined high-edge/downscale source walk case failed\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.word6c = 242u << 16;
    fixture.word70 = 0x2000;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_OK ||
        call.transform.destination_y != 239 ||
        call.transform.source_offset != 11 ||
        call.transform.output_rows != 13 ||
        call.transform.y_accumulator != 0x17ff) {
        fputs("high-edge upscaled row-reuse case failed\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.word70 = 0x40000001;
    fixture.record_scale_x = 0x8000;
    fixture.record_scale_y = 0x8000;
    fixture.word6c = (100u << 16) | (0x80u << 8);
    fixture.frame_width = 0x7fff;
    fixture.frame_height = 0x7fff;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_OK || call.transform.x_scale != 8 ||
        call.transform.y_scale != 8) {
        fputs("low-32-bit MULT truncation case failed\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.facing = -1;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_UNSUPPORTED_STATE) {
        fputs("negative palette branch was not rejected\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.pixel_step = 0;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_UNSUPPORTED_STATE) {
        fputs("zero pixel step was not rejected\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.display_class = 0x40;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_UNSUPPORTED_STATE) {
        fputs("unrecovered display class was not rejected\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.flags = 1;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_UNSUPPORTED_STATE) {
        fputs("unrecovered flag branch was not rejected\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.field8e = 1;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_UNSUPPORTED_STATE) {
        fputs("nonzero record field 0x8e was not rejected\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.field97 = 2;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_UNSUPPORTED_STATE) {
        fputs("nonzero record field 0x97 was not rejected\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.frame_width = 0;
    if (!prepare_synthetic(ram, &fixture, &call, &result) ||
        result != KI15D_TYPE1A_RENDER_INVALID_FRAME) {
        fputs("invalid frame dimensions were not rejected\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.word70 = 0x0800;
    fixture.word6c = 0;
    memset(ram, 0, MAIN_RAM_SIZE);
    KiMemory memory = test_memory(ram, MAIN_RAM_SIZE);
    if (!initialize_guest_state(&memory, &fixture) ||
        ki_memory_write_u8(&memory, fixture.frame_address + 8u, 0) !=
            KI_MEMORY_OK) {
        return 0;
    }
    result = ki15d_88001b90_type1a_prepare_render(
        &memory, fixture.record_address, &call);
    if (result != KI15D_TYPE1A_RENDER_INVALID_FRAME) {
        fputs("zero-sized skipped row was not rejected\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.word6c = 245u << 16;
    if (!prepare_synthetic(ram, &fixture, &call, &result)) {
        return 0;
    }
    memory = test_memory(ram, MAIN_RAM_SIZE);
    if (ki_memory_write_u8(&memory, fixture.frame_address + 8u, 0) !=
            KI_MEMORY_OK ||
        ki15d_88001b90_type1a_prepare_render(
            &memory, fixture.record_address, &call) !=
            KI15D_TYPE1A_RENDER_INVALID_FRAME) {
        fputs("malformed high-edge packed row was not rejected\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    if (!prepare_synthetic(ram, &fixture, &call, &result)) {
        return 0;
    }
    memory = test_memory(ram, MAIN_RAM_SIZE);
    const uint32_t truncated_frame = UINT32_C(0x880ffff8);
    if (ki_memory_write_u32_le(&memory, fixture.record_address + 0x30u,
                               truncated_frame) != KI_MEMORY_OK ||
        ki_memory_write_u16_le(&memory, truncated_frame, 0) != KI_MEMORY_OK ||
        ki_memory_write_u16_le(&memory, truncated_frame + 2u, 0) !=
            KI_MEMORY_OK ||
        ki_memory_write_u16_le(&memory, truncated_frame + 4u,
                               fixture.frame_width) != KI_MEMORY_OK ||
        ki_memory_write_u16_le(&memory, truncated_frame + 6u,
                               fixture.frame_height) != KI_MEMORY_OK ||
        ki15d_88001b90_type1a_prepare_render(
            &memory, fixture.record_address, &call) !=
            KI15D_TYPE1A_RENDER_MEMORY_ERROR) {
        fputs("truncated packed-row walk was not reported\n", stderr);
        return 0;
    }

    if (ki15d_88001b90_type1a_prepare_render(&memory, 0x88110000, &call) !=
        KI15D_TYPE1A_RENDER_MEMORY_ERROR) {
        fputs("unmapped record was not reported\n", stderr);
        return 0;
    }
    if (ki15d_88001b90_type1a_prepare_render(NULL, 0, &call) !=
            KI15D_TYPE1A_RENDER_INVALID_ARGUMENT ||
        ki15d_88001b90_type1a_prepare_render(&memory, 0, NULL) !=
            KI15D_TYPE1A_RENDER_INVALID_ARGUMENT) {
        fputs("invalid bridge arguments were not rejected\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.record_address = 0x88001000;
    fixture.frame_address = 0x88002000;
    memset(ram, 0, MAIN_RAM_SIZE);
    memory = test_memory(ram, MAIN_RAM_SIZE);
    if (!initialize_guest_state(&memory, &fixture)) {
        return 0;
    }
    memory.main_ram_size = 0x00085000;
    if (ki15d_88001b90_type1a_prepare_render(
            &memory, fixture.record_address, &call) !=
        KI15D_TYPE1A_RENDER_MEMORY_ERROR) {
        fputs("unmapped framebuffer selector was not reported\n", stderr);
        return 0;
    }
    memory.main_ram_size = 0x00087000;
    if (ki15d_88001b90_type1a_prepare_render(
            &memory, fixture.record_address, &call) !=
        KI15D_TYPE1A_RENDER_MEMORY_ERROR) {
        fputs("unmapped viewport global was not reported\n", stderr);
        return 0;
    }

    fixture = synthetic_case();
    fixture.frame_address = 0x88110000;
    if (!prepare_synthetic(ram, &fixture, &call, &result)) {
        /* The initializer itself must fail because the requested frame is
         * outside the mapped test RAM; set only the record to exercise the
         * bridge's frame-read error below. */
        memset(ram, 0, MAIN_RAM_SIZE);
        memory = test_memory(ram, MAIN_RAM_SIZE);
        fixture.frame_address = 0x88110000;
        fixture.frame_width = 20;
        if (ki_memory_write_u8(&memory, fixture.record_address, 0x1a) !=
                KI_MEMORY_OK ||
            ki_memory_write_u8(&memory, fixture.record_address + 0x24u, 2) !=
                KI_MEMORY_OK ||
            ki_memory_write_u32_le(&memory, fixture.record_address + 0x30u,
                                   fixture.frame_address) != KI_MEMORY_OK ||
            ki_memory_write_u32_le(&memory, fixture.record_address + 0x68u,
                                   fixture.word68) != KI_MEMORY_OK ||
            ki_memory_write_u32_le(&memory, fixture.record_address + 0x6cu,
                                   fixture.word6c) != KI_MEMORY_OK ||
            ki_memory_write_u32_le(&memory, fixture.record_address + 0x70u,
                                   fixture.word70) != KI_MEMORY_OK ||
            ki_memory_write_u8(&memory, fixture.record_address + 0x94u, 0x60) !=
                KI_MEMORY_OK ||
            ki_memory_write_u16_le(&memory, fixture.record_address + 0xcau,
                                   100) != KI_MEMORY_OK ||
            ki_memory_write_u32_le(&memory, VIEWPORT_HEIGHT_ADDRESS, 240) !=
                KI_MEMORY_OK) {
            return 0;
        }
        result = ki15d_88001b90_type1a_prepare_render(
            &memory, fixture.record_address, &call);
    }
    if (result != KI15D_TYPE1A_RENDER_MEMORY_ERROR) {
        fputs("unmapped frame was not reported\n", stderr);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    DeveloperRenderSource render_source = DEVELOPER_RENDER_COMPARE;
    if (argc == 3 && strcmp(argv[1], "--render-source") == 0) {
        if (strcmp(argv[2], "oracle") == 0) {
            render_source = DEVELOPER_RENDER_ORACLE;
        } else if (strcmp(argv[2], "state") == 0) {
            render_source = DEVELOPER_RENDER_STATE;
        } else if (strcmp(argv[2], "compare") != 0) {
            fprintf(stderr, "unknown render source: %s\n", argv[2]);
            return 2;
        }
    } else if (argc != 1) {
        fprintf(stderr,
                "usage: %s [--render-source oracle|state|compare]\n",
                argv[0]);
        return 2;
    }

    const char *fixture_path =
        "tests/fixtures/ki15d_88001b90_type1a_bridge.csv";
    FILE *input = fopen(fixture_path, "r");
    if (input == NULL) {
        perror(fixture_path);
        return 1;
    }

    uint8_t *ram = malloc(MAIN_RAM_SIZE);
    FixtureCase *fixtures =
        calloc(MAX_FIXTURE_CASES, sizeof(*fixtures));
    Ki15dType1aRenderCall *actual =
        calloc(MAX_FIXTURE_CASES, sizeof(*actual));
    if (ram == NULL || fixtures == NULL || actual == NULL) {
        fputs("could not allocate native bridge test buffers\n", stderr);
        free(actual);
        free(fixtures);
        free(ram);
        fclose(input);
        return 1;
    }

    char line[LINE_CAPACITY];
    unsigned int line_number = 0;
    size_t count = 0;
    int success = 1;
    while (fgets(line, sizeof(line), input) != NULL) {
        line_number++;
        if (line[0] == '#' || strncmp(line, "call,", 5) == 0) {
            continue;
        }
        if (count >= MAX_FIXTURE_CASES) {
            fputs("native bridge fixture has too many rows\n", stderr);
            success = 0;
            break;
        }
        char *fields[FIXTURE_FIELD_COUNT];
        if (!split_fields(line, fields) ||
            !parse_fixture_case(fields, &fixtures[count])) {
            fprintf(stderr, "%s:%u: invalid fixture row\n", fixture_path,
                    line_number);
            success = 0;
            break;
        }

        memset(ram, 0, MAIN_RAM_SIZE);
        KiMemory memory = test_memory(ram, MAIN_RAM_SIZE);
        if (!initialize_guest_state(&memory, &fixtures[count])) {
            fprintf(stderr, "call %" PRIu32 ": could not seed guest state\n",
                    fixtures[count].call_id);
            success = 0;
            break;
        }
        const Ki15dType1aRenderResult result =
            ki15d_88001b90_type1a_prepare_render(
                &memory, fixtures[count].record_address, &actual[count]);
        if (result != KI15D_TYPE1A_RENDER_OK ||
            !compare_call(&fixtures[count], &actual[count])) {
            fprintf(stderr, "call %" PRIu32 ": bridge result=%d\n",
                    fixtures[count].call_id, (int)result);
            success = 0;
            break;
        }
        count++;
    }
    fclose(input);

    if (success && count != MAX_FIXTURE_CASES) {
        fprintf(stderr, "expected %d fixture calls, loaded %zu\n",
                MAX_FIXTURE_CASES, count);
        success = 0;
    }
    uint64_t render_hash = 0;
    if (success && !verify_ordered_render_oracle(
                       fixtures, actual, count, render_source, &render_hash)) {
        success = 0;
    }
    if (success && !verify_boundaries(ram)) {
        success = 0;
    }

    free(actual);
    free(fixtures);
    free(ram);
    if (!success) {
        return 1;
    }
    printf("native type-0x1a render bridge: 315 MAME calls and boundaries "
           "matched; developer render hash=%016" PRIx64 "\n",
           render_hash);
    return 0;
}
