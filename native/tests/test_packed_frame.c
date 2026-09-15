#include "ki/packed_frame.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_valid_frame(void)
{
    /*
     * A small invented frame exercises adjacent runs, an interior transparent
     * gap, a completely transparent row, and the maximum five-bit index.
     */
    const uint8_t packed[] = {
        0xfe, 0xff, 0x03, 0x00, 0x0c, 0x00, 0x03, 0x00,
        0x07, 0x02, 0x19, 0x20, 0x00, 0x02, 0x0b,
        0x01,
        0x04, 0x00, 0xff, 0x13,
    };
    const uint8_t transparent = KI_PACKED_FRAME_TRANSPARENT;
    const uint8_t expected[] = {
        transparent, transparent, 3, 3, 4, transparent,
        transparent, 1, 1, 1, 1, transparent,
        transparent, transparent, transparent, transparent, transparent,
        transparent, transparent, transparent, transparent, transparent,
        transparent, transparent,
        31, 31, 31, 31, 31, 31, 31, 31, 2, 2, 2, 2,
    };
    uint8_t actual[sizeof(expected)] = {0};
    KiPackedFrameInfo info = {0};

    assert(ki_decode_packed_frame(packed, sizeof(packed), actual,
                                  sizeof(actual), &info) ==
           KI_PACKED_FRAME_OK);
    assert(info.origin_x == -2);
    assert(info.origin_y == 3);
    assert(info.width == 12);
    assert(info.height == 3);
    assert(info.bytes_consumed == sizeof(packed));
    assert(memcmp(actual, expected, sizeof(expected)) == 0);
}

static void test_error_paths(void)
{
    const uint8_t header[] = {0, 0, 0, 0, 12, 0, 1, 0};
    const uint8_t missing_skip[] = {
        0, 0, 0, 0, 12, 0, 1, 0, 3, 0, 0,
    };
    const uint8_t row_overrun[] = {
        0, 0, 0, 0, 12, 0, 1, 0, 3, 10, 0x13,
    };
    const uint8_t truncated_row[] = {
        0, 0, 0, 0, 12, 0, 1, 0, 8, 0, 0x08,
    };
    uint8_t output[12] = {0};
    KiPackedFrameInfo info = {0};

    assert(ki_decode_packed_frame(header, 7, output, sizeof(output), &info) ==
           KI_PACKED_FRAME_TRUNCATED);
    assert(ki_decode_packed_frame(header, sizeof(header), output, 11, &info) ==
           KI_PACKED_FRAME_OUTPUT_TOO_SMALL);
    assert(ki_decode_packed_frame(missing_skip, sizeof(missing_skip), output,
                                  sizeof(output), &info) ==
           KI_PACKED_FRAME_INVALID_ROW);
    assert(ki_decode_packed_frame(row_overrun, sizeof(row_overrun), output,
                                  sizeof(output), &info) ==
           KI_PACKED_FRAME_INVALID_ROW);
    assert(ki_decode_packed_frame(truncated_row, sizeof(truncated_row), output,
                                  sizeof(output), &info) ==
           KI_PACKED_FRAME_TRUNCATED);
}

int main(void)
{
    test_valid_frame();
    test_error_paths();
    puts("native packed-frame decoder tests: ok");
    return 0;
}
