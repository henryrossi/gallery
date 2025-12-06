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
        if (node != NULL) {                                                    \
                SLLStackPop(state.nameLower##Stack.free);                      \
        } else {                                                               \
                node = arena_alloc(ui_build_arena(),                           \
                                   sizeof(UI##nameUpper##Node));               \
        }                                                                      \
        node->v = value;                                                       \
        SLLStackPush(state.nameLower##Stack.top, node);

// clang-format off

UIStackFuncImpl()

// hr: temp function
static Vec2 get_screen_size(void) {
        Vec2 res = { .x = 1000.0f, .y = 800.0f };
        return res;
}
// clang-format on

static Arena *ui_build_arena(void) {
        Arena *res = &ui_state.arena;
        return res;
}

static inline u32 ui_element_list_is_empty(UIElementList *list) {
        return list->first == NULL;
}

static void ui_element_list_append(Arena *arena, UIElementList *list,
                                   UIElement *element) {
        UIElementNode *node = arena_alloc(arena, sizeof(UIElementNode));
        node->element = element;
        node->next = NULL;

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
                list->last = NULL;
        }
        return res;
}

static UIElement *ui_element_list_pop_last(UIElementList *list) {
        UIElement *res = list->last->element;

        if (list->first == list->last) {
                list->first = NULL;
                list->last = NULL;
        } else {
                UIElementNode *penult = list->first;
                while (penult->next != list->last) {
                        penult = penult->next;
                }
                penult->next = NULL;
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

static SemanticSize semanticSize(UI_SIZEKIND kind, f32 value, f32 strictness) {
        SemanticSize res = {
                .kind = kind,
                .value = value,
                .strictness = strictness,
        };
        return res;
}

static void setup_ui_state() {
        ui_state.arena = make_arena(0xF0000);
        u64 elementSize = sizeof(UIElement *);

        ui_state.bucketCount = 0xF00;
        ui_state.buckets
            = arena_alloc(&ui_state.arena, elementSize * ui_state.bucketCount);

        UIElement *root = arena_alloc(&ui_state.arena, sizeof(UIElement));
        root->parent = NULL;
        root->next = NULL;
        root->prev = NULL;
        root->firstChild = NULL;
        root->lastChild = NULL;
        root->layoutDirection = UI_AXIS2D_Y;

        ui_state.root = root;

        ui_state.parentStackBottom.v = root;
        ui_state.textColorStackBottom.v = vec4(1, 1, 1, 1);
        ui_state.backgroundColorStackBottom.v = vec4(1, 1, 1, 1);
        ui_state.widthStackBottom.v
            = semanticSize(UI_SIZEKIND_PercentOfParent, 100, 0);
        ui_state.heightStackBottom.v
            = semanticSize(UI_SIZEKIND_PercentOfParent, 100, 0);

        ui_state.parentStack.top = &ui_state.parentStackBottom;
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
        if (existing == NULL) {
                ui_state.buckets[index] = element;
        } else {
                while (existing->hashNext) {
                        existing = existing->hashNext;
                }
                existing->hashNext = element;
                element->hashPrev = existing;
        }
}

static void ui_cache_prune(u64 frame) {
        for (u64 i = 0; i < ui_state.bucketCount; i++) {
                UIElement *e = ui_state.buckets[i];
                while (e) {
                        if (e->lastFrameTouched < frame) {
                                if (e->hashPrev == NULL) {
                                        ui_state.buckets[i] = e->hashNext;
                                } else {
                                        e->hashPrev->hashNext = e->hashNext;
                                }
                                if (e->hashNext) {
                                        e->hashNext->hashPrev = e->hashPrev;
                                }
                                // e->hashPrev = NULL;
                                // e->hashNext = NULL;
                                // hr: We should reclaim empty UIElements
                                //     instead of just leaking memory
                        }
                        e = e->hashNext;
                }
        }
}

static b32 ui_key_match(u64 a, u64 b) {
        b32 res = a == b;
        return res;
}

static UIElement *ui_build_element_from_key(UI_ELEMENTFLAGS flags, u64 key) {
        UIElement *res = ui_cache_lookup(key);

        if (res == NULL) {
                res = arena_alloc(&ui_state.arena, sizeof(UIElement));
                res->key = key;
        }

        UIElement *parent = ui_top_parent();

        res->parent = parent;
        res->next = NULL;
        res->prev = parent->lastChild;
        res->firstChild = NULL;
        res->lastChild = NULL;

        if (!parent->firstChild) {
                parent->firstChild = res;
        }
        if (parent->lastChild) {
                parent->lastChild->next = res;
        }
        parent->lastChild = res;

        res->flags = flags;
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
        return NULL;
}

/* UI autolayout algorithm functions */

static void ui_autolayout_calc_preorder(UIElement *e, UI_AXIS2D axis) {
        SemanticSize size = e->size[axis];
        f32 computedSize = 0.0f;
        f32 parentSize = 0.0f;

        switch (size.kind) {
        case UI_SIZEKIND_Pixels:
                computedSize = size.value;
                break;
        case UI_SIZEKIND_TextContent:
                // hr: TODO
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
        SemanticSize size = e->size[axis];
        f32 computedSize = 0.0f;

        if (size.kind == UI_SIZEKIND_SumOfChildren) {
                for (UIElement *child = e->firstChild; child;
                     child = child->next) {
                        computedSize += (axis == UI_AXIS2D_X)
                                            ? child->computedSize.x
                                            : child->computedSize.y;
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
        Vec2 screenExtent = get_screen_size();
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
        //     Here we can apply margins, which will also be relevant to the
        //     violation calculation above
        arena_reset(&scratch);
        queue.first = NULL;
        queue.last = NULL;
        ui_element_list_append(&scratch, &queue, root);
        while (!ui_element_list_is_empty(&queue)) {
                UIElement *cur = ui_element_list_pop_first(&queue);
                for (UIElement *child = cur->firstChild; child;
                     child = child->next) {
                        ui_element_list_append(&scratch, &queue, child);
                        if (!child->prev) {
                                child->relPosition.x = 0.0f;
                                child->relPosition.y = 0.0f;
                                continue;
                        }
                        UIElement *prev = child->prev;
                        if (root->layoutDirection == UI_AXIS2D_X) {
                                child->relPosition.x = prev->relPosition.x
                                                       + prev->computedSize.x;
                        } else {
                                child->relPosition.y = prev->relPosition.y
                                                       + prev->computedSize.y;
                        }
                }
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
