#include "ui/generated/ui.c"

__thread UIState ui_state;

#define SLLStackPop_N(head, next) ((head) = (head)->next)
#define SLLStackPush_N(head, node, next)                                       \
        ((node)->next = (head), (head) = (node))

#define SLLStackPop(head) SLLStackPop_N(head, next)
#define SLLStackPush(head, node) SLLStackPush_N(head, node, next)

#define UIStackPopImpl(state, nameUpper, nameLower)                            \
        UI##nameUpper##Node *node = state.nameLower##Stack.top;                \
        if (node != &state.nameLower##StackBottom) {                           \
                SLLStackPop(state.nameLower##Stack.top);                       \
                SLLStackPush(state.nameLower##Stack.free, node);               \
        }                                                                      \
        return node->v;

#define UIStackTopImpl(state, nameUpper, nameLower)                            \
        return state.nameLower##Stack.top->v;

#define UIStackPushImpl(state, nameUpper, nameLower, value)                    \
        UI##nameUpper##Node *node = state.nameLower##Stack.free;               \
        if (node != 0) {                                                       \
                SLLStackPop(state.nameLower##Stack.free);                      \
        } else {                                                               \
                node = arena_alloc(ui_build_arena(),                           \
                                   sizeof(UI##nameUpper##Node));               \
        }                                                                      \
        node->v = value;                                                       \
        SLLStackPush(state.nameLower##Stack.top, node);

// clang-format off

UIStackFuncImpl()

static Arena *ui_build_arena(void) {
        Arena *res = &ui_state.arena;
        return res;
}
// clang-format on

static inline u32 ui_element_list_is_empty(UIElementList *list) {
        return list->first == 0;
}

static void ui_element_list_append(Arena *arena, UIElementList *list,
                                   UIElement *element) {
        UIElementNode *node = arena_alloc(arena, sizeof(UIElementNode));
        node->element = element;
        node->next = 0;

        if (ui_element_list_is_empty(list)) {
                list->first = node;
                list->last = node;
        } else {
                list->last->next = node;
                list->last = node;
        }
}

static UIElement *ui_element_list_pop_first(UIElementList *list) {
        UIElement *res = list->first->element;
        list->first = list->first->next;
        if (ui_element_list_is_empty(list)) {
                list->last = 0;
        }
        return res;
}

static UIElement *ui_element_list_pop_last(UIElementList *list) {
        UIElement *res = list->last->element;

        if (list->first == list->last) {
                list->first = 0;
                list->last = 0;
        } else {
                UIElementNode *penult = list->first;
                while (penult->next != list->last) {
                        penult = penult->next;
                }
                penult->next = 0;
                list->last = penult;
        }

        return res;
}

static String8 ui_find_hash_within_string(String8 str) {
        String8 res = str;
        u64 hashSignifierPos = string8_find_substr(str, string8_lit("##"));
        if (hashSignifierPos < str.length) {
                res = string8_skip(str, hashSignifierPos);
        }
        return res;
}

static String8 ui_find_text_within_string(String8 str) {
        String8 res = str;
        u64 hashSignifierPos = string8_find_substr(str, string8_lit("##"));
        if (hashSignifierPos < str.length) {
                res = string8_prune(str, hashSignifierPos);
        }
        return res;
}

static UISemanticSize uiSemanticSize(UI_SIZEKIND kind, f32 value,
                                     f32 strictness) {
        UISemanticSize res = {
                .kind = kind,
                .value = value,
                .strictness = strictness,
        };
        return res;
}

static void setup_ui_state() {
        ui_state.arena = make_arena(0xF0000);
        ui_state.strArena = make_arena(0xF0000);

        ui_state.defaultFont
            = f_init_font(string8_lit("resources/RobotoMono-Regular.ttf"));
        assert(ui_state.defaultFont && "Failed to load default font");

        u64 elementSize = sizeof(UIElement *);

        ui_state.bucketCount = 0xF00;
        ui_state.buckets
            = arena_alloc(&ui_state.arena, elementSize * ui_state.bucketCount);
        ui_state.eFree = 0;

        UIElement *root = arena_alloc(&ui_state.arena, sizeof(UIElement));
        root->parent = 0;
        root->next = 0;
        root->prev = 0;
        root->firstChild = 0;
        root->lastChild = 0;
        root->layoutDirection = UI_AXIS2D_Y;
        root->lastFrameTouched = UINT64_MAX;

        ui_state.root = root;

        ui_state.parentStackBottom.v = root;
        ui_state.textSizeStackBottom.v = 32.0f;
        ui_state.textColorStackBottom.v = v4f32(0, 0, 0, 1);
        ui_state.backgroundColorStackBottom.v = v4f32(1, 1, 1, 1);
        ui_state.widthStackBottom.v
            = uiSemanticSize(UI_SIZEKIND_PercentOfParent, 100, 0);
        ui_state.heightStackBottom.v
            = uiSemanticSize(UI_SIZEKIND_PercentOfParent, 100, 0);

        ui_state.parentStack.top = &ui_state.parentStackBottom;
        ui_state.textSizeStack.top = &ui_state.textSizeStackBottom;
        ui_state.textColorStack.top = &ui_state.textColorStackBottom;
        ui_state.backgroundColorStack.top
            = &ui_state.backgroundColorStackBottom;
        ui_state.widthStack.top = &ui_state.widthStackBottom;
        ui_state.heightStack.top = &ui_state.heightStackBottom;
}

static UIElement *ui_cache_lookup(u64 key) {
        u64 index = key % ui_state.bucketCount;
        UIElement *res = ui_state.buckets[index];
        while (res && res->key != key) {
                res = res->hashNext;
        }
        return res;
}

static void ui_cache_add(UIElement *element) {
        u64 index = element->key % ui_state.bucketCount;
        UIElement *existing = ui_state.buckets[index];
        if (existing == 0) {
                ui_state.buckets[index] = element;
                element->hashNext = 0;
                element->hashPrev = 0;
        } else {
                while (existing->hashNext) {
                        existing = existing->hashNext;
                }
                existing->hashNext = element;
                element->hashPrev = existing;
                element->hashNext = 0;
        }
}

static void ui_cache_prune(u64 frame) {
        for (u64 i = 0; i < ui_state.bucketCount; i++) {
                UIElement *e = ui_state.buckets[i];
                while (e) {
                        UIElement *next = e->hashNext;
                        if (e->lastFrameTouched < frame) {
                                if (e->hashPrev == 0) {
                                        ui_state.buckets[i] = e->hashNext;
                                } else {
                                        e->hashPrev->hashNext = e->hashNext;
                                }
                                if (e->hashNext) {
                                        e->hashNext->hashPrev = e->hashPrev;
                                }

                                e->hashPrev = 0;
                                e->hashNext = ui_state.eFree;
                                ui_state.eFree = e;
                        }
                        e = next;
                }
        }
}

static b32 ui_key_match(u64 a, u64 b) {
        b32 res = a == b;
        return res;
}

static UIElement *ui_build_element_from_key(UI_ELEMENTFLAGS flags, u64 key) {
        UIElement *res = ui_cache_lookup(key);

        if (res == 0) {
                if (ui_state.eFree) {
                        res = ui_state.eFree;
                        ui_state.eFree = res->hashNext;
                } else {
                        res = arena_alloc(&ui_state.arena, sizeof(UIElement));
                }
                res->key = key;
                ui_cache_add(res);
        }

        UIElement *parent = ui_top_parent();

        res->parent = parent;
        res->next = 0;
        res->prev = parent->lastChild;
        res->firstChild = 0;
        res->lastChild = 0;

        if (!parent->firstChild) {
                parent->firstChild = res;
        }
        if (parent->lastChild) {
                parent->lastChild->next = res;
        }
        parent->lastChild = res;

        res->flags = flags;
        res->layoutDirection
            = parent->layoutDirection; // NOTE: hr: this needs more thought
        res->size[UI_AXIS2D_X] = ui_top_width();
        res->size[UI_AXIS2D_Y] = ui_top_height();
        res->textSize = ui_top_text_size();
        res->textColor = ui_top_text_color();
        res->backgroundColor = ui_top_background_color();
        return res;
}

static UIElement *ui_build_element_from_string(UI_ELEMENTFLAGS flags,
                                               String8 str) {
        String8 hashStr = ui_find_hash_within_string(str);
        u64 key = string8_hashkey(hashStr);
        UIElement *res = ui_build_element_from_key(flags, key);
        res->text = ui_find_text_within_string(str);
        return res;
}

static UIElement *ui_build_element_from_stringf(UI_ELEMENTFLAGS flags,
                                                char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        String8 str = string8fv(&ui_state.strArena, fmt, args);
        UIElement *res = ui_build_element_from_string(flags, str);
        va_end(args);
        return res;
}

static UIElement *ui_build_element_from_stringfv(UI_ELEMENTFLAGS flags,
                                                 char *fmt, va_list args) {
        String8 str = string8fv(&ui_state.strArena, fmt, args);
        UIElement *res = ui_build_element_from_string(flags, str);
        return res;
}

/* UI autolayout algorithm functions */

static void ui_autolayout_calc_preorder(UIElement *e, UI_AXIS2D axis) {
        UISemanticSize size = e->size[axis];
        f32 computedSize = 0.0f;
        f32 parentSize = 0.0f;

        switch (size.kind) {
        case UI_SIZEKIND_Pixels:
                computedSize = size.value;
                break;
        case UI_SIZEKIND_TextContent:
                if (axis == UI_AXIS2D_X) {
                        computedSize = f_text_length(ui_state.defaultFont,
                                                     e->textSize, e->text);
                } else {
                        // TODO: hr: we need an algorithm that can compute how
                        // many lines of text a paragraph is given it's width.
                        computedSize = e->textSize;
                }
                break;
        case UI_SIZEKIND_PercentOfParent:
                parentSize = (axis == UI_AXIS2D_X) ? e->parent->computedSize.x
                                                   : e->parent->computedSize.y;
                computedSize = parentSize * size.value / 100.0f;
                break;
        default:
                return;
        }

        if (axis == UI_AXIS2D_X) {
                e->computedSize.x = computedSize;
        } else {
                e->computedSize.y = computedSize;
        }
}

static void ui_autolayout_calc_postorder(UIElement *e, UI_AXIS2D axis) {
        UISemanticSize size = e->size[axis];
        f32 computedSize = 0.0f;

        if (size.kind == UI_SIZEKIND_SumOfChildren) {
                for (UIElement *child = e->firstChild; child;
                     child = child->next) {
                        if (axis == e->layoutDirection) {
                                computedSize += (axis == UI_AXIS2D_X)
                                                    ? child->computedSize.x
                                                    : child->computedSize.y;
                        } else {
                                f32 childSize = (axis == UI_AXIS2D_X)
                                                    ? child->computedSize.x
                                                    : child->computedSize.y;
                                computedSize = max(computedSize, childSize);
                        }
                }

                if (axis == UI_AXIS2D_X) {
                        e->computedSize.x = computedSize;
                } else {
                        e->computedSize.y = computedSize;
                }
        }
}

static void ui_autolayout_rec_postorder(UIElement *e) {
        if (!e) {
                return;
        }

        for (UIElement *child = e->firstChild; child; child = child->next) {
                ui_autolayout_rec_postorder(child);
        }

        ui_autolayout_calc_postorder(e, UI_AXIS2D_X);
        ui_autolayout_calc_postorder(e, UI_AXIS2D_Y);
}

static void ui_element_autolayout(void) {
        Arena scratch = make_arena(OS_PAGESIZE);
        Vec2f32 screenExtent = r_get_window_size();
        UIElement *root = ui_state.root;
        root->size[UI_AXIS2D_X].kind = UI_SIZEKIND_Pixels;
        root->size[UI_AXIS2D_X].value = screenExtent.x;
        root->size[UI_AXIS2D_Y].kind = UI_SIZEKIND_Pixels;
        root->size[UI_AXIS2D_Y].value = screenExtent.y;

        UIElementList queue = { 0 };
        ui_element_list_append(&scratch, &queue, root);
        while (!ui_element_list_is_empty(&queue)) {
                UIElement *e = ui_element_list_pop_first(&queue);
                UIElement *child = e->firstChild;

                ui_autolayout_calc_preorder(e, UI_AXIS2D_X);
                ui_autolayout_calc_preorder(e, UI_AXIS2D_Y);

                while (child) {
                        ui_element_list_append(&scratch, &queue, child);
                        child = child->next;
                }
        }

        ui_autolayout_rec_postorder(root);

        // hr: Solve violations where children exceed parent

        // hr: Compute relative positions and on screen coords
        //     Here we can apply margins, which will also be relevant to
        //     the violation calculation above
        root->screenCoords.p0 = v2f32(0, 0);
        root->screenCoords.p1 = root->computedSize;
        u64 frame = r_get_frame_count();

        arena_reset(&scratch);
        queue.first = 0;
        queue.last = 0;
        ui_element_list_append(&scratch, &queue, root);
        while (!ui_element_list_is_empty(&queue)) {
                UIElement *cur = ui_element_list_pop_first(&queue);
                cur->lastFrameTouched = frame;
                for (UIElement *child = cur->firstChild; child;
                     child = child->next) {
                        ui_element_list_append(&scratch, &queue, child);
                        if (!child->prev) {
                                child->relPosition.x = 0.0f;
                                child->relPosition.y = 0.0f;
                        } else {
                                UIElement *prev = child->prev;
                                if (root->layoutDirection == UI_AXIS2D_X) {
                                        child->relPosition.x
                                            = prev->relPosition.x
                                              + prev->computedSize.x;
                                } else {
                                        child->relPosition.y
                                            = prev->relPosition.y
                                              + prev->computedSize.y;
                                }
                        }
                        child->screenCoords.p0 = add_2f32(
                            child->parent->screenCoords.p0, child->relPosition);
                        child->screenCoords.p1 = add_2f32(
                            child->computedSize, child->screenCoords.p0);
                }
        }
}

static void ui_draw_element_rec(UIElement *e) {
        dr_rect(e->screenCoords, e->backgroundColor, 0, 0);
        dr_text(ui_state.defaultFont, e->textSize, e->text, e->screenCoords,
                e->textColor);
        for (UIElement *child = e->firstChild; child; child = child->next) {
                ui_draw_element_rec(child);
        }
}

static void ui_draw_elements(void) {
        ui_draw_element_rec(ui_state.root);

        r_dispatch_batch();

        u64 frame = r_get_frame_count();
        ui_cache_prune(frame);

        ui_state.root->firstChild = 0;
        ui_state.root->lastChild = 0;
        // reset everything
        while (ui_state.parentStack.top != &ui_state.parentStackBottom) {
                ui_pop_parent();
        }
        while (ui_state.widthStack.top != &ui_state.widthStackBottom) {
                ui_pop_width();
        }
        while (ui_state.heightStack.top != &ui_state.heightStackBottom) {
                ui_pop_height();
        }
        while (ui_state.textColorStack.top != &ui_state.textColorStackBottom) {
                ui_pop_text_color();
        }
        while (ui_state.backgroundColorStack.top
               != &ui_state.backgroundColorStackBottom) {
                ui_pop_background_color();
        }
}

static void ui_element_add_display_string(UIElement *e, String8 str) {
        e->text = str;
}

static void ui_element_add_child_layout_axis(UIElement *e, UI_AXIS2D axis) {
        e->layoutDirection = axis;
}

static UISignal ui_signal_from_element(UIElement *e) {
        UISignal sig = { .element = e };
        return sig;
}
