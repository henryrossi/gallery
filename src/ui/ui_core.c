#ifndef XXH_IMPLEMENTATION
#define XXH_IMPLEMENTATION
#include "thirdparty/xxHash/xxh3.h"
#endif

// hr: temp function
static Vec2 get_screen_size(void) {
        Vec2 res = { .x = 1000.0f, .y = 800.0f };
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

static UIState uiState;

static void setup_ui_state() {
        uiState.arena = make_arena(0xF0000);
        u64 elementSize = sizeof(UIElement *);

        uiState.stackCount = 20;
        uiState.stack
            = arena_alloc(&uiState.arena, elementSize * uiState.stackCount);

        uiState.bucketCount = 0xF00;
        uiState.buckets
            = arena_alloc(&uiState.arena, elementSize * uiState.bucketCount);

        UIElement *root = arena_alloc(&uiState.arena, sizeof(UIElement));
        root->parent = NULL;
        root->next = NULL;
        root->prev = NULL;
        root->firstChild = NULL;
        root->lastChild = NULL;
        root->layoutDirection = AXIS2D_Y;

        uiState.root = root;
}

static void ui_push_parent(UIElement *e) {
        uiState.stack[uiState.stackTop] = e;
        uiState.stackTop++;
}

static UIElement *ui_get_top_parent(void) {
        if (uiState.stackTop == 0) {
                return uiState.root;
        }
        return uiState.stack[uiState.stackTop - 1];
}

static UIElement *ui_pop_parent(void) {
        UIElement *res = NULL;
        if (uiState.stackTop > 0) {
                res = uiState.stack[--uiState.stackTop];
        }
        return res;
}

static UIElement *ui_cache_lookup(u64 key) {
        u64 index = key % uiState.bucketCount;
        UIElement *res = uiState.buckets[index];
        while (res && res->key != key) {
                res = res->hashNext;
        }
        return res;
}

static void ui_cache_add(UIElement *element) {
        u64 index = element->key % uiState.bucketCount;
        UIElement *existing = uiState.buckets[index];
        if (existing == NULL) {
                uiState.buckets[index] = element;
        } else {
                while (existing->hashNext) {
                        existing = existing->hashNext;
                }
                existing->hashNext = element;
                element->hashPrev = existing;
        }
}

static void ui_cache_prune(u64 frame) {
        for (u64 i = 0; i < uiState.bucketCount; i++) {
                UIElement *e = uiState.buckets[i];
                while (e) {
                        if (e->lastFrameTouched < frame) {
                                if (e->hashPrev == NULL) {
                                        uiState.buckets[i] = e->hashNext;
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

static UIElement *ui_build_element_from_key(u64 key) {
        UIElement *res = ui_cache_lookup(key);

        if (res == NULL) {
                res = arena_alloc(&uiState.arena, sizeof(UIElement));
                res->key = key;
        }

        UIElement *parent = ui_get_top_parent();

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

        return res;
}

static UIElement *ui_build_element_from_string(String8 str) {
        String8 hashStr = ui_find_hash_from_string(str);
        u64 key = string8_hashkey(hashStr);
        UIElement *res = ui_build_element_from_key(key);
        res->text = ui_find_text_from_string(str);
        return res;
}

static void ui_autolayout_calc_preorder(UIElement *e, Axis2D axis) {
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
                parentSize = (axis == AXIS2D_X) ? e->parent->computedSize.x
                                                : e->parent->computedSize.y;
                computedSize = parentSize * size.value / 100.0f;
                break;
        default:
                return;
        }

        if (axis == AXIS2D_X) {
                e->computedSize.x = computedSize;
        } else {
                e->computedSize.y = computedSize;
        }
}

static void ui_autolayout_calc_postorder(UIElement *e, Axis2D axis) {
        SemanticSize size = e->size[axis];
        f32 computedSize = 0.0f;

        if (size.kind == UI_SIZEKIND_SumOfChildren) {
                for (UIElement *child = e->firstChild; child;
                     child = child->next) {
                        computedSize += (axis == AXIS2D_X)
                                            ? child->computedSize.x
                                            : child->computedSize.y;
                }

                if (axis == AXIS2D_X) {
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

        ui_autolayout_calc_postorder(e, AXIS2D_X);
        ui_autolayout_calc_postorder(e, AXIS2D_Y);
}

static void ui_element_autolayout(void) {
        Arena scratch = make_arena(OS_PAGESIZE);
        Vec2 screenExtent = get_screen_size();
        UIElement *root = uiState.root;
        root->size[AXIS2D_X].kind = UI_SIZEKIND_Pixels;
        root->size[AXIS2D_X].value = screenExtent.x;
        root->size[AXIS2D_Y].kind = UI_SIZEKIND_Pixels;
        root->size[AXIS2D_Y].value = screenExtent.y;

        UIElementList queue = { 0 };
        ui_element_list_append(&scratch, &queue, root);
        while (!ui_element_list_is_empty(&queue)) {
                UIElement *e = ui_element_list_pop_first(&queue);
                UIElement *child = e->firstChild;

                ui_autolayout_calc_preorder(e, AXIS2D_X);
                ui_autolayout_calc_preorder(e, AXIS2D_Y);

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
                        if (root->layoutDirection == AXIS2D_X) {
                                child->relPosition.x = prev->relPosition.x
                                                       + prev->computedSize.x;
                        } else {
                                child->relPosition.y = prev->relPosition.y
                                                       + prev->computedSize.y;
                        }
                }
        }
}
