#include "bedrock/bedrock_inc.h"

static inline void glf_print_create_usage() {
        fprintf(stdout, "Usage for creating new image:\n"
                        "   .glyph -c <filename> <width> <height>\n");
}

typedef struct {
        const char *filename;
        u32 width;
        u32 height;
        b32 valid;
} GLFArgs;

static GLFArgs glf_parse_command_line_args(int argc, char *argv[]) {
        GLFArgs res = { 0 };
        if (argc < 2) {
                fprintf(stdout, "Usages:\n"
                                "   ./glyph <filename>\n"
                                "   ./glyph -c <filename> <width> <height>\n");
                return res;
        }

        if (strncmp(argv[1], "-c", 2) == 0) {
                if (argc < 5) {
                        glf_print_create_usage();
                        return res;
                }

                res.filename = argv[2];
                res.width = strtol(argv[3], 0, 10);
                res.height = strtol(argv[4], 0, 10);
                if (res.height == 0 || res.width == 0) {
                        glf_print_create_usage();
                        return res;
                }
        } else {
                res.filename = argv[1];
        }
        res.valid = 1;
        return res;
}
