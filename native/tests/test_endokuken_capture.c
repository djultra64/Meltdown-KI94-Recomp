#include "ki/endokuken_capture.h"

#include <assert.h>
#include <stdio.h>

static const size_t FRAME_OFFSETS[] = {
    0x97670u, 0x97778u, 0x97898u, 0x979c8u, 0x97aecu, 0x97c30u,
    0x97d50u, 0x97ea4u, 0x97fecu, 0x981a0u, 0x98394u, 0x985b8u,
    0x98808u, 0x98ab4u, 0x98da0u, 0x990e0u, 0x993dcu, 0x995ecu,
};

int main(void)
{
    size_t next_sample = 0;
    unsigned int maximum_particles = 0;

    for (size_t tick = 0; tick < KI_ENDOKUKEN_CAPTURE_FRAME_COUNT; tick++) {
        const KiEndokukenCaptureFrame *frame =
            &KI_ENDOKUKEN_CAPTURE_FRAMES[tick];
        assert(frame->first_sample == next_sample);
        assert(frame->sample_count >= 1 && frame->sample_count <= 6);
        if (frame->sample_count > maximum_particles) {
            maximum_particles = frame->sample_count;
        }

        for (size_t index = 0; index < frame->sample_count; index++) {
            const KiEndokukenCaptureSample *sample =
                &KI_ENDOKUKEN_CAPTURE_SAMPLES[next_sample + index];
            assert(sample->token >= 0x04 && sample->token <= 0x15);
            assert(sample->frame_offset == FRAME_OFFSETS[sample->token - 0x04]);
            assert(sample->object_address >= 0x8808be00u);
            assert(sample->object_address <= 0x8808c300u);
            assert((sample->object_address & 0xffu) == 0);
            assert(sample->transform.x_direction == 1);
            assert(sample->transform.y_direction == -1);
            assert(sample->transform.x_scale != 0);
            assert(sample->transform.y_scale != 0);
            assert(sample->transform.source_offset >= 8);
            assert(sample->transform.output_rows != 0);
        }
        next_sample += frame->sample_count;
    }

    assert(next_sample == KI_ENDOKUKEN_CAPTURE_SAMPLE_COUNT);
    assert(maximum_particles == 6);
    printf("native Endokuken capture: %zu frames, %zu calls, peak %u particles\n",
           (size_t)KI_ENDOKUKEN_CAPTURE_FRAME_COUNT,
           (size_t)KI_ENDOKUKEN_CAPTURE_SAMPLE_COUNT, maximum_particles);
    return 0;
}
