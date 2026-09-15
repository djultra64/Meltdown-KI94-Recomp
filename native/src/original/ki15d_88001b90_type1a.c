#include "ki/original/ki15d_88001b90_type1a.h"

#include <limits.h>
#include <stddef.h>

enum {
    TYPE1A_RECORD_TYPE = 0x1a,
    DISPLAY_CLASS_PACKED = 0x60,
    FRAME_HEADER_SIZE = 8,
    FIXED_ONE = 0x1000,
    FIXED_MASK = FIXED_ONE - 1,
    SCREEN_WIDTH = 320,
    FRAMEBUFFER_BANK_BYTES = 0x28000,
    PALETTE_BANK_BYTES = 0x100
};

static const uint32_t FRAMEBUFFER_SELECT_ADDRESS = UINT32_C(0x88086200);
static const uint32_t VIEWPORT_HEIGHT_ADDRESS = UINT32_C(0x88087b40);
static const uint32_t FRAMEBUFFER_BASE = UINT32_C(0x80030000);
static const uint32_t PALETTE_BANK_BASE = UINT32_C(0x88090100);
static const uint32_t LIVE_PALETTE_TABLE = UINT32_C(0x8803480a);
static const uint32_t FORWARD_ROW_CALLBACK = UINT32_C(0x88011918);
static const uint32_t REVERSE_ROW_CALLBACK = UINT32_C(0x88011a24);
static const uint32_t FORWARD_CLIPPED_ROW_CALLBACK = UINT32_C(0x880118c0);
static const uint32_t REVERSE_CLIPPED_ROW_CALLBACK = UINT32_C(0x880119c4);

typedef struct Type1aState {
    uint8_t type;
    int8_t pixel_step;
    uint32_t frame_address;
    uint16_t record_scale_x;
    uint16_t record_scale_y;
    uint32_t word68;
    uint32_t word6c;
    uint32_t word70;
    int8_t facing;
    uint8_t field8e;
    uint8_t display_class;
    uint8_t flags;
    uint8_t field97;
    uint16_t horizontal_minimum;
    uint16_t horizontal_limit;
    uint8_t framebuffer_select;
    uint32_t viewport_height;
} Type1aState;

static uint32_t add_u32(uint32_t left, uint32_t right)
{
    return left + right;
}

static uint32_t subtract_u32(uint32_t left, uint32_t right)
{
    return left - right;
}

/* R4600 MULT writes only the low 32 bits used by the following MFLO. */
static uint32_t signed_product_low(uint32_t left, uint32_t right)
{
    const int64_t product =
        (int64_t)(int32_t)left * (int64_t)(int32_t)right;
    return (uint32_t)(uint64_t)product;
}

static uint32_t resolve_scale(uint32_t base, uint16_t record_scale)
{
    if (record_scale == 0) {
        return base;
    }
    return signed_product_low(base, record_scale) >> 12;
}

static int vertical_accumulator_crossed(uint32_t accumulator)
{
    /* The renderer uses signed SLT ($k0 < accumulator), not SLTU. */
    return (int32_t)accumulator > FIXED_MASK;
}

static int32_t arithmetic_shift_right_8(uint32_t value)
{
    /* Division plus an explicit negative remainder models MIPS SRA without
     * depending on the host compiler's signed-right-shift convention. */
    const int32_t signed_value = (int32_t)value;
    int32_t quotient = signed_value / 256;
    if (signed_value < 0 && signed_value % 256 != 0) {
        quotient--;
    }
    return quotient;
}

static int32_t arithmetic_shift_right_12(uint32_t value)
{
    const int32_t signed_value = (int32_t)value;
    int32_t quotient = signed_value / 4096;
    if (signed_value < 0 && signed_value % 4096 != 0) {
        quotient--;
    }
    return quotient;
}

static int read_state(const KiMemory *memory,
                      uint32_t record,
                      Type1aState *state)
{
    uint8_t pixel_step = 0;
    uint8_t facing = 0;
    if (ki_memory_read_u8(memory, record, &state->type) != KI_MEMORY_OK ||
        ki_memory_read_u8(memory, record + 0x24u, &pixel_step) != KI_MEMORY_OK ||
        ki_memory_read_u32_le(memory, record + 0x30u,
                              &state->frame_address) != KI_MEMORY_OK ||
        ki_memory_read_u16_le(memory, record + 0x58u,
                              &state->record_scale_x) != KI_MEMORY_OK ||
        ki_memory_read_u16_le(memory, record + 0x5au,
                              &state->record_scale_y) != KI_MEMORY_OK ||
        ki_memory_read_u32_le(memory, record + 0x68u, &state->word68) !=
            KI_MEMORY_OK ||
        ki_memory_read_u32_le(memory, record + 0x6cu, &state->word6c) !=
            KI_MEMORY_OK ||
        ki_memory_read_u32_le(memory, record + 0x70u, &state->word70) !=
            KI_MEMORY_OK ||
        ki_memory_read_u8(memory, record + 0x8cu, &facing) != KI_MEMORY_OK ||
        ki_memory_read_u8(memory, record + 0x8eu, &state->field8e) !=
            KI_MEMORY_OK ||
        ki_memory_read_u8(memory, record + 0x94u, &state->display_class) !=
            KI_MEMORY_OK ||
        ki_memory_read_u8(memory, record + 0x95u, &state->flags) !=
            KI_MEMORY_OK ||
        ki_memory_read_u8(memory, record + 0x97u, &state->field97) !=
            KI_MEMORY_OK ||
        ki_memory_read_u16_le(memory, record + 0xc8u,
                              &state->horizontal_minimum) != KI_MEMORY_OK ||
        ki_memory_read_u16_le(memory, record + 0xcau,
                              &state->horizontal_limit) != KI_MEMORY_OK ||
        ki_memory_read_u8(memory, FRAMEBUFFER_SELECT_ADDRESS,
                          &state->framebuffer_select) != KI_MEMORY_OK ||
        ki_memory_read_u32_le(memory, VIEWPORT_HEIGHT_ADDRESS,
                              &state->viewport_height) != KI_MEMORY_OK) {
        return 0;
    }
    state->pixel_step = (int8_t)pixel_step;
    state->facing = (int8_t)facing;
    return 1;
}

static int read_frame_header(const KiMemory *memory,
                             uint32_t frame_address,
                             int16_t *origin_x,
                             int16_t *origin_y,
                             int16_t *width,
                             int16_t *height)
{
    uint16_t raw_origin_x = 0;
    uint16_t raw_origin_y = 0;
    uint16_t raw_width = 0;
    uint16_t raw_height = 0;
    if (ki_memory_read_u16_le(memory, frame_address, &raw_origin_x) !=
            KI_MEMORY_OK ||
        ki_memory_read_u16_le(memory, frame_address + 2u, &raw_origin_y) !=
            KI_MEMORY_OK ||
        ki_memory_read_u16_le(memory, frame_address + 4u, &raw_width) !=
            KI_MEMORY_OK ||
        ki_memory_read_u16_le(memory, frame_address + 6u, &raw_height) !=
            KI_MEMORY_OK) {
        return 0;
    }
    *origin_x = (int16_t)raw_origin_x;
    *origin_y = (int16_t)raw_origin_y;
    *width = (int16_t)raw_width;
    *height = (int16_t)raw_height;
    return 1;
}

static Ki15dType1aRenderResult consume_packed_source_row(
    const KiMemory *memory,
    uint32_t frame_address,
    uint16_t frame_height,
    uint32_t *next_offset,
    uint32_t *consumed_rows,
    uint32_t *selected_offset)
{
    if (*consumed_rows >= frame_height) {
        return KI15D_TYPE1A_RENDER_INVALID_FRAME;
    }
    if (UINT32_MAX - frame_address < *next_offset) {
        return KI15D_TYPE1A_RENDER_INVALID_FRAME;
    }

    uint8_t row_size = 0;
    if (ki_memory_read_u8(memory, frame_address + *next_offset, &row_size) !=
        KI_MEMORY_OK) {
        return KI15D_TYPE1A_RENDER_MEMORY_ERROR;
    }
    if (row_size == 0 || UINT32_MAX - *next_offset < row_size) {
        return KI15D_TYPE1A_RENDER_INVALID_FRAME;
    }

    *selected_offset = *next_offset;
    *next_offset += row_size;
    (*consumed_rows)++;
    return KI15D_TYPE1A_RENDER_OK;
}

static Ki15dType1aRenderResult select_first_visible_source_row(
    const KiMemory *memory,
    uint32_t frame_address,
    uint16_t frame_height,
    int32_t raw_destination_y,
    uint32_t viewport_height,
    uint32_t scaled_height,
    uint32_t vertical_fraction,
    uint32_t y_scale,
    uint32_t *source_offset,
    uint32_t *y_accumulator,
    uint32_t *output_rows)
{
    /* 0x88010b44 rejects a positive-stride span whose lower edge never
     * reaches the viewport. Keep the unclamped anchor for this test. */
    const int32_t far_edge = (int32_t)subtract_u32(
        (uint32_t)raw_destination_y, scaled_height);
    if (far_edge >= (int32_t)viewport_height) {
        return KI15D_TYPE1A_RENDER_NOT_VISIBLE;
    }

    const uint32_t rows_above_minimum = (uint32_t)raw_destination_y + 1u;
    uint32_t rows = scaled_height < rows_above_minimum
                        ? scaled_height
                        : rows_above_minimum;
    uint32_t accumulator = vertical_fraction;
    uint32_t next_offset = FRAME_HEADER_SIZE;
    uint32_t selected_offset = FRAME_HEADER_SIZE;
    uint32_t consumed_rows = 0;
    int32_t clip_y = raw_destination_y;
    int selected = 0;

    /* 0x88010b78-0x88010bd8 advances packed source rows for every output
     * line hidden below the high viewport edge. A large y scale can reuse one
     * selected source row for more than one rejected output line. */
    while (clip_y >= (int32_t)viewport_height) {
        while (!vertical_accumulator_crossed(accumulator)) {
            const Ki15dType1aRenderResult result =
                consume_packed_source_row(memory, frame_address, frame_height,
                                          &next_offset, &consumed_rows,
                                          &selected_offset);
            if (result != KI15D_TYPE1A_RENDER_OK) {
                return result;
            }
            selected = 1;
            accumulator = add_u32(accumulator, y_scale);
        }

        clip_y--;
        if (rows == 0) {
            return KI15D_TYPE1A_RENDER_NOT_VISIBLE;
        }
        rows--;
        if (clip_y < (int32_t)viewport_height) {
            break;
        }
        accumulator = subtract_u32(accumulator, FIXED_ONE);
    }

    if (rows == 0) {
        return KI15D_TYPE1A_RENDER_NOT_VISIBLE;
    }

    /* The common packed loop begins at 0x8801180c. High clipping arrives
     * with an already selected row; the ordinary path starts scanning at the
     * first row after the frame header. */
    if (selected) {
        accumulator = subtract_u32(accumulator, FIXED_ONE);
    }
    if (!selected || !vertical_accumulator_crossed(accumulator)) {
        do {
            const Ki15dType1aRenderResult result =
                consume_packed_source_row(memory, frame_address, frame_height,
                                          &next_offset, &consumed_rows,
                                          &selected_offset);
            if (result != KI15D_TYPE1A_RENDER_OK) {
                return result;
            }
            accumulator = add_u32(accumulator, y_scale);
        } while (!vertical_accumulator_crossed(accumulator));
    }

    *source_offset = selected_offset;
    *y_accumulator = accumulator;
    *output_rows = rows;
    return KI15D_TYPE1A_RENDER_OK;
}

static uint32_t row_callback_for_mode(uint8_t render_mode)
{
    /* The four entries repeat for every scale class in the original table at
     * 0x88035500: forward, reverse, forward-clipped, reverse-clipped. */
    switch (render_mode & 0x0cu) {
    case 0x00:
        return FORWARD_ROW_CALLBACK;
    case 0x04:
        return REVERSE_ROW_CALLBACK;
    case 0x08:
        return FORWARD_CLIPPED_ROW_CALLBACK;
    default:
        return REVERSE_CLIPPED_ROW_CALLBACK;
    }
}

static Ki15dType1aRenderResult classify_horizontal_span(
    int32_t x,
    uint32_t scaled_width,
    int direction,
    uint16_t minimum,
    uint16_t limit,
    Ki15dHorizontalClip *clip)
{
    const int64_t far_edge =
        (int64_t)x + (int64_t)direction * (int64_t)scaled_width;
    int left = 0;
    int right = 0;

    if (direction > 0) {
        if (x >= limit || far_edge <= minimum) {
            return KI15D_TYPE1A_RENDER_NOT_VISIBLE;
        }
        left = x < minimum;
        right = far_edge > limit;
    } else {
        if (x < minimum || far_edge >= limit) {
            return KI15D_TYPE1A_RENDER_NOT_VISIBLE;
        }
        left = far_edge < minimum;
        right = x >= limit;
    }

    *clip = (Ki15dHorizontalClip)((left ? KI15D_HORIZONTAL_CLIP_LEFT : 0) |
                                  (right ? KI15D_HORIZONTAL_CLIP_RIGHT : 0));
    return KI15D_TYPE1A_RENDER_OK;
}

Ki15dType1aRenderResult ki15d_88001b90_type1a_prepare_render(
    const KiMemory *memory,
    uint32_t record_address,
    Ki15dType1aRenderCall *call)
{
    if (memory == NULL || call == NULL || memory->main_ram == NULL) {
        return KI15D_TYPE1A_RENDER_INVALID_ARGUMENT;
    }

    Type1aState state = {0};
    if (!read_state(memory, record_address, &state)) {
        return KI15D_TYPE1A_RENDER_MEMORY_ERROR;
    }
    if (state.type != TYPE1A_RECORD_TYPE ||
        state.display_class != DISPLAY_CLASS_PACKED || state.flags != 0 ||
        state.facing < 0 || state.pixel_step == 0 || state.field8e != 0 ||
        state.field97 != 0) {
        return KI15D_TYPE1A_RENDER_UNSUPPORTED_STATE;
    }

    if (state.horizontal_limit == 0) {
        state.horizontal_limit = SCREEN_WIDTH;
    }
    if (state.horizontal_minimum >= state.horizontal_limit ||
        state.viewport_height == 0 || state.viewport_height > INT32_MAX) {
        return KI15D_TYPE1A_RENDER_UNSUPPORTED_STATE;
    }

    int16_t origin_x = 0;
    int16_t origin_y = 0;
    int16_t frame_width = 0;
    int16_t frame_height = 0;
    if (!read_frame_header(memory, state.frame_address, &origin_x, &origin_y,
                           &frame_width, &frame_height)) {
        return KI15D_TYPE1A_RENDER_MEMORY_ERROR;
    }
    if (frame_width <= 0 || frame_height <= 0) {
        return KI15D_TYPE1A_RENDER_INVALID_FRAME;
    }

    const uint32_t x_scale = resolve_scale(state.word70, state.record_scale_x);
    const uint32_t y_scale = resolve_scale(state.word70, state.record_scale_y);
    if (x_scale == 0 || y_scale == 0) {
        return KI15D_TYPE1A_RENDER_UNSUPPORTED_STATE;
    }

    const int32_t x_position = arithmetic_shift_right_8(state.word68);
    const int32_t y_position = arithmetic_shift_right_8(state.word6c);
    const uint32_t y_origin_product =
        signed_product_low((uint32_t)(int32_t)origin_y, y_scale);
    const uint32_t y_base_fixed =
        add_u32((uint32_t)y_position << 4, UINT32_C(0x800));
    const uint32_t y_with_origin = add_u32(y_origin_product, y_base_fixed);
    const int32_t raw_destination_y = arithmetic_shift_right_12(y_with_origin);
    if (raw_destination_y < 0) {
        return KI15D_TYPE1A_RENDER_NOT_VISIBLE;
    }
    int32_t destination_y = raw_destination_y;
    if (destination_y >= (int32_t)state.viewport_height) {
        destination_y = (int32_t)state.viewport_height - 1;
    }

    const uint32_t scaled_width =
        signed_product_low((uint32_t)(int32_t)frame_width, x_scale) >> 12;
    const uint32_t scaled_height =
        signed_product_low((uint32_t)(int32_t)frame_height, y_scale) >> 12;
    if (scaled_width == 0 || scaled_height == 0) {
        return KI15D_TYPE1A_RENDER_NOT_VISIBLE;
    }

    const uint32_t x_origin_product =
        signed_product_low((uint32_t)(int32_t)origin_x, x_scale);
    const uint32_t x_base_fixed =
        add_u32((uint32_t)x_position << 4, UINT32_C(0x800));
    const int x_direction = state.pixel_step > 0 ? 1 : -1;
    const uint32_t x_fixed =
        x_direction > 0 ? subtract_u32(x_base_fixed, x_origin_product)
                        : add_u32(x_base_fixed, x_origin_product);
    const int32_t destination_x = arithmetic_shift_right_12(x_fixed);
    const uint32_t x_fraction = x_fixed & FIXED_MASK;
    const uint32_t x_remainder =
        x_direction > 0 ? x_fraction : x_fraction ^ FIXED_MASK;

    Ki15dHorizontalClip horizontal_clip = KI15D_HORIZONTAL_CLIP_NONE;
    Ki15dType1aRenderResult result = classify_horizontal_span(
        destination_x, scaled_width, x_direction, state.horizontal_minimum,
        state.horizontal_limit, &horizontal_clip);
    if (result != KI15D_TYPE1A_RENDER_OK) {
        return result;
    }

    const uint32_t vertical_fraction =
        (y_with_origin & FIXED_MASK) ^ FIXED_MASK;
    uint32_t source_offset = 0;
    uint32_t y_accumulator = 0;
    uint32_t output_rows = 0;
    result = select_first_visible_source_row(
        memory, state.frame_address, (uint16_t)frame_height, raw_destination_y,
        state.viewport_height, scaled_height, vertical_fraction, y_scale,
        &source_offset, &y_accumulator, &output_rows);
    if (result != KI15D_TYPE1A_RENDER_OK) {
        return result;
    }

    uint8_t render_mode = state.display_class;
    if (x_scale > FIXED_ONE) {
        render_mode = (uint8_t)(render_mode + 0x10u);
    } else if (x_scale < FIXED_ONE) {
        render_mode = (uint8_t)(render_mode + 0x20u);
    }
    if (x_direction < 0) {
        render_mode = (uint8_t)(render_mode + 0x04u);
    }
    if (horizontal_clip != KI15D_HORIZONTAL_CLIP_NONE) {
        render_mode = (uint8_t)(render_mode | 0x08u);
    }

    const uint8_t framebuffer_bank = state.framebuffer_select & 1u;
    const Ki15dType1aRenderCall prepared = {
        .frame_address = state.frame_address,
        .palette_source_address =
            PALETTE_BANK_BASE + (uint32_t)(uint8_t)state.facing * PALETTE_BANK_BYTES,
        .palette_table_address = LIVE_PALETTE_TABLE,
        .framebuffer_address =
            FRAMEBUFFER_BASE + framebuffer_bank * FRAMEBUFFER_BANK_BYTES,
        .row_callback_address = row_callback_for_mode(render_mode),
        .frame_width = (uint16_t)frame_width,
        .frame_height = (uint16_t)frame_height,
        .scaled_width = scaled_width,
        .framebuffer_bank = framebuffer_bank,
        .render_mode = render_mode,
        .horizontal_clip = horizontal_clip,
        .transform = {
            .destination_x = destination_x,
            .destination_y = destination_y,
            .x_direction = x_direction,
            .y_direction = -1,
            .x_scale = x_scale,
            .x_remainder = x_remainder,
            .y_scale = y_scale,
            .y_accumulator = y_accumulator,
            .source_offset = source_offset,
            .output_rows = output_rows,
        },
    };
    *call = prepared;
    return KI15D_TYPE1A_RENDER_OK;
}
