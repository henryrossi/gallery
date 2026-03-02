#include <string.h>

typedef struct {
        u32 pos;
        Vec4u8 color;
} GLDrawingActionChange;

typedef struct GLHistoryState GLHistoryState;
struct GLHistoryState {
        GLHistoryState *next;
        GLHistoryState *prev;
        GLDrawingActionChange *execute;
        GLDrawingActionChange *revert;
        u32 size;
};

static DrawingActionChange *staging = NULL;
static uint32_t stagingSize = 0;

static arena a = { 0 };

static HistoryState *currentActionState = NULL;

static int initActionHistory(Canvas *c) {
        staging
            = malloc(sizeof(DrawingActionChange) * c->width * c->height * 2);
        if (!staging) {
                return 0;
        }

        if (!constructArena(&a)) {
                return 0;
        }

        // currentActionState = malloc(sizeof(HistoryState));
        currentActionState = arenaAllocate(&a, sizeof(HistoryState));
        if (!currentActionState) {
                return 0;
        }
        currentActionState->size = 0;

        return 1;
}

static void recordDrawingAction(DrawingActionChange *execute,
                                DrawingActionChange *revert) {
        assert(execute->pos % 4 == 0
               && "Drawing Action being recorded at unaligned canvas position");
        assert(execute->pos == revert->pos
               && "Drawing Action execute and revert at different canvas "
                  "positions");

        uint32_t i = stagingSize * 2;
        staging[i].pos = execute->pos;
        staging[i].r = execute->r;
        staging[i].g = execute->g;
        staging[i].b = execute->b;
        staging[i].a = execute->a;

        i++;
        staging[i].pos = revert->pos;
        staging[i].r = revert->r;
        staging[i].g = revert->g;
        staging[i].b = revert->b;
        staging[i].a = revert->a;

        stagingSize++;
}

static int finalizeDrawingAction(void) {
        // HistoryState *state = malloc(sizeof(HistoryState));
        // DrawingActionChange *exe
        //     = malloc(sizeof(DrawingActionChange) * stagingSize);
        // DrawingActionChange *rev
        //     = malloc(sizeof(DrawingActionChange) * stagingSize);
        arenaResetAt(&a, currentActionState->next);
        HistoryState *state = arenaAllocate(&a, sizeof(HistoryState));
        DrawingActionChange *exe
            = arenaAllocate(&a, sizeof(DrawingActionChange) * stagingSize);
        DrawingActionChange *rev
            = arenaAllocate(&a, sizeof(DrawingActionChange) * stagingSize);

        if (!exe || !rev || !state) {
                fprintf(stderr,
                        "Failed to allocate memory for DrawingAction\n");
                return 0;
        }

        for (uint32_t i = 0; i < stagingSize; i++) {
                exe[i] = staging[i * 2];
                rev[i] = staging[i * 2 + 1];
        }

        state->prev = currentActionState;
        state->next = NULL;
        state->execute = exe;
        state->revert = rev;
        state->size = stagingSize;

        currentActionState->next = state;
        currentActionState = state;

        stagingSize = 0;

        return 1;
}

static void undoAction(Canvas *c) {
        if (currentActionState->size == 0)
                return;

        assert(currentActionState->prev && "Action state has no prev action");

        for (uint32_t i = 0; i < currentActionState->size; i++) {
                DrawingActionChange a = currentActionState->revert[i];
                c->data[a.pos] = a.r;
                c->data[a.pos + 1] = a.g;
                c->data[a.pos + 2] = a.b;
                c->data[a.pos + 3] = a.a;
        }
        currentActionState = currentActionState->prev;
}

static void redoAction(Canvas *c) {
        if (!currentActionState->next)
                return;

        currentActionState = currentActionState->next;
        for (uint32_t i = 0; i < currentActionState->size; i++) {
                DrawingActionChange a = currentActionState->execute[i];
                c->data[a.pos] = a.r;
                c->data[a.pos + 1] = a.g;
                c->data[a.pos + 2] = a.b;
                c->data[a.pos + 3] = a.a;
        }
}

static void cleanupActionHistory(void) { free(staging); }
