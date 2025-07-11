#include "glyph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void printCreateUsage() {
        fprintf(stdout, "Usage for creating new image:\n"
                        "   .glyph -c <filename> <width> <height>\n");
}

static int parseCommandLineArgs(glyph_state *state, int argc, char *argv[]) {
        if (argc < 2) {
                fprintf(stdout, "Usages:\n"
                                "   ./glyph <filename>\n"
                                "   ./glyph -c <filename> <width> <height>\n");
                return 0;
        }

        if (strncmp(argv[1], "-c", 2) == 0) {
                if (argc < 5) {
                        printCreateUsage();
                        return 0;
                }

                state->canvas.filename = argv[2];
                state->canvas.fileCreated = 1;

                long w = strtol(argv[3], NULL, 10);
                if (!w) {
                        printCreateUsage();
                        return 0;
                }
                long h = strtol(argv[4], NULL, 10);
                if (!h) {
                        printCreateUsage();
                        return 0;
                }

                state->canvas.width = w;
                state->canvas.height = h;
        } else {
                state->canvas.filename = argv[1];
        }
        return 1;
}
