#include "ki/original/ki15d_8802d5b0.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { LINE_CAPACITY = 160 };

int main(void)
{
    /*
     * Keep the MAME-produced values outside the test executable. This makes
     * the CSV the single auditable fixture shared by the native test and the
     * per-function provenance record.
     */
    const char *fixture_path = "tests/fixtures/ki15d_8802d5b0.csv";
    FILE *fixture = fopen(fixture_path, "r");
    if (fixture == NULL) {
        perror(fixture_path);
        return 1;
    }

    char line[LINE_CAPACITY];
    unsigned int line_number = 0;
    unsigned int cases_checked = 0;
    while (fgets(line, sizeof(line), fixture) != NULL) {
        line_number++;

        /* Allow the fixture to explain its origin and retain a column header. */
        if (line[0] == '#' || line[0] == '\n' ||
            strncmp(line, "input_hex,", 10) == 0) {
            continue;
        }

        uint64_t input = 0;
        uint64_t expected = 0;
        if (sscanf(line, "%" SCNx64 ",%" SCNx64, &input, &expected) != 2) {
            fprintf(stderr, "%s:%u: invalid fixture row\n", fixture_path,
                    line_number);
            fclose(fixture);
            return 1;
        }

        const uint64_t actual = ki15d_8802d5b0(input);

        /* Stop at the first mismatch so the offending input is unambiguous. */
        if (actual != expected) {
            fprintf(stderr,
                    "%s:%u: input=%016" PRIx64 " expected=%016" PRIx64
                    " actual=%016" PRIx64 "\n",
                    fixture_path, line_number, input, expected, actual);
            fclose(fixture);
            return 1;
        }
        cases_checked++;
    }

    if (ferror(fixture)) {
        perror(fixture_path);
        fclose(fixture);
        return 1;
    }
    fclose(fixture);

    if (cases_checked == 0) {
        fprintf(stderr, "%s: no fixture cases found\n", fixture_path);
        return 1;
    }

    printf("native routine 0x8802d5b0: %u MAME cases matched\n",
           cases_checked);
    return 0;
}
