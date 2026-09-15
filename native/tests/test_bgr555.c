#include "ki/bgr555.h"

#include <stdio.h>
#include <string.h>

enum { LINE_CAPACITY = 256 };

int main(void)
{
    const char *fixture_path =
        "tests/fixtures/ki15d_bgr555_saturating_add.csv";
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
        if (line[0] == '#' || line[0] == '\n' ||
            strncmp(line, "destination,", 12) == 0) {
            continue;
        }

        unsigned int destination = 0;
        unsigned int blend = 0;
        unsigned int expected = 0;
        if (sscanf(line, "%x,%x,%x", &destination, &blend, &expected) != 3 ||
            destination > 0xffffu || blend > 0xffffu ||
            expected > 0xffffu) {
            fprintf(stderr, "%s:%u: invalid fixture row\n", fixture_path,
                    line_number);
            fclose(fixture);
            return 1;
        }

        const uint16_t actual = ki_bgr555_saturating_add(
            (uint16_t)destination, (uint16_t)blend);
        if (actual != expected) {
            fprintf(stderr,
                    "%s:%u: %04x + %04x expected=%04x actual=%04x\n",
                    fixture_path, line_number, destination, blend, expected,
                    actual);
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

    printf("native BGR555 saturating add: %u MAME cases matched\n",
           cases_checked);
    return 0;
}
