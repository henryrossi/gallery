#include "glyph.h"

#include "actionHistory.c"

int testRecordActionHistory() {
        Canvas c = { 0 };
        uint32_t width = 64;
        uint32_t height = 64;
        uint32_t size = width * height * 4;
        c.data = calloc(size, 1);
        if (!c.data || !initActionHistory(&c)) {
                fprintf(stderr, "Failed to allocate memory for test\n");
                return 0;
        }

        uint32_t pos = 0;
        recordDrawingAction(pos, &c);

        c.data[0] = 255;
        c.data[1] = 255;
        c.data[2] = 255;
        c.data[3] = 255;

        endAction();

        assert(head && "Head is NULL");
        assert(head->size == 1 && "Head size should be 1 but is not");
        assert(head->prev == NULL && "Head prev is not NULL");
        assert(head->next == NULL && "Head next is not NULL");
        assert(head->changes->pos == 0 && "Head changes[0] pos is not 0");
        assert(head->changes->r == 0 && "Head changes[0] r is not 0");
        assert(head->changes->g == 0 && "Head changes[0] g is not 0");
        assert(head->changes->b == 0 && "Head changes[0] b is not 0");
        assert(head->changes->a == 0 && "Head changes[0] a is not 0");

        undoAction(&c);

        return 0;
}

int main(int argc, char *argv[]) { testRecordActionHistory(); }
