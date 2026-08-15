#include "ui/ui_core.h"
#include "draw/draw.h"
#include "os/os.h"

#include "ui/generated/ui.c"

// NOTE: hr: for dropdowns, I think they should be removed from the main root
// tree, but still have a parent. They can be held in a seperate list and
// treated as their own boxes to be laid out

// clang-format off
UIStackNodesDecl

typedef struct {
        Arena *arena;
        Arena *perFrameArena;
        Freelist freelist;

        FFont *defaultFont;

        UIElement *root;
        u32 numElements;

        u64 bucketCount;
        UIElement **buckets;
        UIElement *eFree;

        UIElement *focused;
        String8 *focusedText;

        #define UI_CHAR_INPUT_BUF_LENGTH 120
        String8 charInputBuf;

        Vec2f32 prevMousePos;
        u32 prevMouseState[UI_Button_Count];
        Vec2f32 pressOrigin[UI_Button_Count];
        u64 pressedElementKey[UI_Button_Count];
        u64 prevClickFrame[UI_Button_Count];
        Vec2f32 prevClick[UI_Button_Count];

        UIStacksDecl
} UIState;

// clang-format on

static UIState ui_state = { .focused = &ui_nil_element };

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
        SLLStackPush(state.nameLower##Stack.top, node);                        \
        state.nameLower##Stack.autoPop = 0;

#define UIStackSetNextImpl(state, nameUpper, nameLower, value)                 \
        UI##nameUpper##Node *node = state.nameLower##Stack.free;               \
        if (node != 0) {                                                       \
                SLLStackPop(state.nameLower##Stack.free);                      \
        } else {                                                               \
                node = arena_alloc(ui_build_arena(),                           \
                                   sizeof(UI##nameUpper##Node));               \
        }                                                                      \
        node->v = value;                                                       \
        SLLStackPush(state.nameLower##Stack.top, node);                        \
        state.nameLower##Stack.autoPop = 1;

// clang-format off

UIStackFuncImpl()

static Arena *ui_build_arena(void) {
        Arena *res = ui_state.arena;
        return res;
}
// clang-format on

static Arena *ui_frame_arena(void) {
        Arena *res = ui_state.perFrameArena;
        return res;
}

static Freelist *ui_freelist(void) {
        Freelist *res = &ui_state.freelist;
        return res;
}

static b32 ui_element_is_nil(UIElement *e) {
        return e == &ui_nil_element;
}

// hr: UIElement list utility
typedef struct UIElementNode UIElementNode;
struct UIElementNode {
        UIElement *element;
        UIElementNode *next;
};

readonly static UIElementNode ui_nil_element_node = {
        &ui_nil_element,
        &ui_nil_element_node,
};

typedef struct {
        UIElementNode *first;
        UIElementNode *last;
        u64 count;
} UIElementList;

static b32 ui_element_list_is_empty(UIElementList *list) {
        return list->count == 0;
}

static void ui_element_list_push(Arena *a, UIElementList *list, UIElement *e) {
        UIElementNode *node = arena_alloc(a, sizeof(*node));
        node->element = e;
        SLLQueuePush_NZ(&ui_nil_element_node, list->first, list->last, node,
                        next);
        list->count++;
}

static UIElement *ui_element_list_dequeue(UIElementList *list) {
        UIElement *res = list->first->element;
        SLLQueuePop_NZ(&ui_nil_element_node, list->first, list->last, next);
        list->count--;
        return res;
}

static Vec2f32 ui_content_scale(void) {
        Vec2f32 res = { .x = 1, .y = 1 };
        glfwGetWindowContentScale(r_state.window, &res.x, &res.y);
        return res;
}

static Vec2f32 ui_mouse_pos(void) {
        f64 xpos = 0;
        f64 ypos = 0;
        glfwGetCursorPos(r_state.window, &xpos, &ypos);

        Vec2f32 res = mul_v2f32(v2f32(xpos, ypos), ui_content_scale());
        return res;
}

static Vec2f32 ui_drag_delta(void) {
        Vec2f32 res = sub_v2f32(ui_mouse_pos(), ui_state.prevMousePos);
        return res;
}

static void ui_set_focused(UIElement *e, String8 *text) {
        ui_state.focused = e;
        ui_state.focusedText = text;
}

static UIElement *ui_get_focused(void) {
        return ui_state.focused;
}

static UISemanticSize uiSemanticSize(UI_SizeKind kind, f32 value,
                                     f32 strictness) {
        UISemanticSize res = {
                .kind = kind,
                .value = value,
                .strictness = strictness,
        };
        return res;
}

static f32 ui_scale_value(f32 value, UI_Axis2d scaledBy) {
        Vec2f32 scale = ui_content_scale();
        return scaledBy == UI_Axis2d_X   ? value * scale.x
               : scaledBy == UI_Axis2d_Y ? value * scale.y
                                         : value;
}

static void ui_char_callback(GLFWwindow *window, u32 codepoint) {
        String8 s = ui_state.charInputBuf;
        if (ui_element_is_nil(ui_get_focused())
            || s.length + 4 > UI_CHAR_INPUT_BUF_LENGTH) {
                return;
        }

        if (codepoint <= 0x7F) {
                s.length = 1;
                string8_set(s, 0, codepoint);
        } else if (codepoint <= 0x7FF) {
                s.length = 2;
                string8_set(s, 0, 0xC0 | (codepoint >> 6));
                string8_set(s, 1, 0x80 | (codepoint & 0x3F));
        } else if (codepoint <= 0xFFFF) {
                s.length = 3;
                string8_set(s, 0, 0xE0 | (codepoint >> 12));
                string8_set(s, 1, 0x80 | ((codepoint >> 6) & 0x3F));
                string8_set(s, 2, 0x80 | (codepoint & 0x3F));
        } else {
                s.length = 4;
                string8_set(s, 0, 0xF0 | (codepoint >> 18));
                string8_set(s, 1, 0x80 | ((codepoint >> 12) & 0x3F));
                string8_set(s, 2, 0x80 | ((codepoint >> 6) & 0x3F));
                string8_set(s, 3, 0x80 | (codepoint & 0x3F));
        }

        ui_state.charInputBuf = s;
}

static void setup_ui_state(void) {
        glfwSetCharCallback(r_state.window, ui_char_callback);

        ui_state.arena = make_arena(mb(1));
        ui_state.perFrameArena = make_arena(mb(1));
        ui_state.freelist = make_freelist(mb(1));

        ui_state.defaultFont
            = f_init_font(string8_lit("resources/RobotoMono-Regular.ttf"));
        if (!ui_state.defaultFont) {
                log_message(string8_lit("Failed to load default font\n"));
                os_abort(1);
        }

        ui_state.bucketCount = 0xF00;
        u64 tableSize = sizeof(UIElement *) * ui_state.bucketCount;
        ui_state.buckets = arena_alloc(ui_state.arena, tableSize);
        for (u64 i = 0; i < ui_state.bucketCount; i++) {
                ui_state.buckets[i] = &ui_nil_element;
        }
        ui_state.eFree = &ui_nil_element;

        ui_state.charInputBuf.data
            = arena_alloc(ui_state.arena, UI_CHAR_INPUT_BUF_LENGTH);

        for (u32 i = 0; i < UI_Button_Count; i++) {
                ui_state.pressOrigin[i] = v2f32(-1.0f, -1.0f);
                ui_state.pressedElementKey[i] = 0;
        }

        UIElement *root = arena_alloc(ui_state.arena, sizeof(UIElement));
        root->parent = &ui_nil_element;
        root->next = &ui_nil_element;
        root->prev = &ui_nil_element;
        root->firstChild = &ui_nil_element;
        root->lastChild = &ui_nil_element;
        root->layoutDirection = UI_Axis2d_Y;
        root->lastFrameTouched = UINT64_MAX;

        ui_state.root = root;

        ui_state.parentStackBottom.v = root;
        ui_state.textSizeStackBottom.v = 16.0f;
        ui_state.textColorStackBottom.v = v4f32(0, 0, 0, 1);
        ui_state.backgroundColorStackBottom.v = v4f32(1, 1, 1, 1);
        ui_state.borderColorStackBottom.v = v4f32(0, 0, 0, 1);
        ui_state.borderSizeStackBottom.v = 1.0f;
        ui_state.cornerRadiusStackBottom.v = 0;
        ui_state.paddingStackBottom.v = v4f32(0, 0, 0, 0);
        ui_state.widthStackBottom.v
            = uiSemanticSize(UI_SizeKind_PercentOfParent, 100, 1);
        ui_state.heightStackBottom.v
            = uiSemanticSize(UI_SizeKind_PercentOfParent, 100, 1);

        ui_state.parentStack.top = &ui_state.parentStackBottom;
        ui_state.textSizeStack.top = &ui_state.textSizeStackBottom;
        ui_state.textColorStack.top = &ui_state.textColorStackBottom;
        ui_state.backgroundColorStack.top
            = &ui_state.backgroundColorStackBottom;
        ui_state.borderColorStack.top = &ui_state.borderColorStackBottom;
        ui_state.borderSizeStack.top = &ui_state.borderSizeStackBottom;
        ui_state.cornerRadiusStack.top = &ui_state.cornerRadiusStackBottom;
        ui_state.paddingStack.top = &ui_state.paddingStackBottom;
        ui_state.widthStack.top = &ui_state.widthStackBottom;
        ui_state.heightStack.top = &ui_state.heightStackBottom;
}

static b32 ui_begin_frame(void) {
        b32 res = r_begin_frame();

        glfwPollEvents();
        // glfwWaitEvents();

        if (!ui_element_is_nil(ui_get_focused())) {
                Freelist *fl = ui_freelist();
                String8 fText = *ui_state.focusedText;

                if (ui_state.charInputBuf.length) {
                        *ui_state.focusedText = string8_concat_f(
                            fl, fText, ui_state.charInputBuf);
                        // clang-format off
// WARN:                  ∆
// hr: if this allocation | fails, what are my options? Ideally we have a 
// strong guarantee of memory preallocation (we know about failure as soon as
// possible) as with an arena, but this is tricky due to potential memory 
// fragmentation. What to do? Merge consecutive blocks? What if that's not 
// possible because fragmentation is awful. Copy to a temp buffer and layout 
// more compactly? In this specific case, if I can keep total allocation a 
// fraction of allocator size I don't foresee many issues (hopefully).
// For further discussion see: "The Easiest Way to Handle Errors Is To Not Have
// Them" Ryan Fleury
                        // clang-format on
                        if (freelist_contains_mem(fl, fText.data)) {
                                string8_destroy_f(fl, fText);
                        }
                }
        }

        ui_state.charInputBuf.length = 0;

        return res;
}

static UIElement *ui_cache_lookup(u64 key) {
        UIElement *res = &ui_nil_element;

        if (!ui_key_match(key, 0)) {
                u64 index = key % ui_state.bucketCount;
                for (UIElement *e = ui_state.buckets[index];
                     !ui_element_is_nil(e); e = e->hashNext) {
                        if (ui_key_match(key, e->key)) {
                                res = e;
                                break;
                        }
                }
        }
        return res;
}

static void ui_element_cache_add(UIElement *element) {
        if (!ui_element_is_nil(element)) {
                u64 index = element->key % ui_state.bucketCount;
                UIElement *existing = ui_state.buckets[index];
                if (ui_element_is_nil(existing)) {
                        ui_state.buckets[index] = element;
                        element->hashNext = &ui_nil_element;
                        element->hashPrev = &ui_nil_element;
                } else {
                        while (!ui_element_is_nil(existing->hashNext)) {
                                existing = existing->hashNext;
                        }
                        existing->hashNext = element;
                        element->hashPrev = existing;
                        element->hashNext = &ui_nil_element;
                }
        }
}

static void ui_element_cache_prune(u64 frame) {
        for (u64 i = 0; i < ui_state.bucketCount; i++) {
                UIElement *e = ui_state.buckets[i];
                while (!ui_element_is_nil(e)) {
                        UIElement *next = e->hashNext;
                        if (e->lastFrameTouched < frame) {
                                if (ui_element_is_nil(e->hashPrev)) {
                                        ui_state.buckets[i] = e->hashNext;
                                } else {
                                        e->hashPrev->hashNext = e->hashNext;
                                }
                                if (!ui_element_is_nil(e->hashNext)) {
                                        e->hashNext->hashPrev = e->hashPrev;
                                }

                                e->hashPrev = &ui_nil_element;
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

#define ui_auto_pop_stack(name, _name)                                         \
        if (ui_state.name##Stack.autoPop) {                                    \
                ui_pop_##_name();                                              \
                ui_state.name##Stack.autoPop = 0;                              \
        }
static UIElement *ui_build_element_from_key(UI_ElementFlags flags, u64 key) {
        UIElement *res = ui_cache_lookup(key);
        if (ui_element_is_nil(res)) {
                if (ui_element_is_nil(ui_state.eFree)) {
                        res = arena_alloc(ui_state.arena, sizeof(UIElement));
                } else {
                        res = ui_state.eFree;
                        ui_state.eFree = res->hashNext;
                }
                res->key = key;
                ui_element_cache_add(res);
        }

        UIElement *parent = ui_top_parent();

        res->parent = parent;

        res->next = &ui_nil_element;
        res->prev = parent->lastChild;
        if (ui_element_is_nil(parent->firstChild)) {
                parent->firstChild = res;
        }
        if (!ui_element_is_nil(parent->lastChild)) {
                parent->lastChild->next = res;
        }
        parent->lastChild = res;

        res->firstChild = &ui_nil_element;
        res->lastChild = &ui_nil_element;

        res->flags = flags;
        res->layoutDirection = parent->layoutDirection; // NOTE: hr: this needs
                                                        // more thought
        res->size[UI_Axis2d_X] = ui_top_width();
        res->size[UI_Axis2d_Y] = ui_top_height();
        res->textSize = ui_top_text_size();
        res->textColor = ui_top_text_color();
        Vec4f32 bg = ui_top_background_color();
        res->backgroundColors[0] = bg;
        res->backgroundColors[1] = bg;
        res->backgroundColors[2] = bg;
        res->backgroundColors[3] = bg;
        res->borderColor = ui_top_border_color();
        res->borderSize = ui_top_border_size();
        res->cornerRadius = ui_top_corner_radius();
        res->padding = ui_top_padding();
        res->texture = 0;

        // hr: auto pop stacks
        ui_auto_pop_stack(parent, parent);
        ui_auto_pop_stack(width, width);
        ui_auto_pop_stack(height, height);
        ui_auto_pop_stack(textSize, text_size);
        ui_auto_pop_stack(textColor, text_color);
        ui_auto_pop_stack(backgroundColor, background_color);
        ui_auto_pop_stack(borderColor, border_color);
        ui_auto_pop_stack(borderSize, border_size);
        ui_auto_pop_stack(cornerRadius, corner_radius);
        ui_auto_pop_stack(padding, padding);

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

static UIElement *ui_build_element_from_string(UI_ElementFlags flags,
                                               String8 str) {
        String8 hashStr = ui_find_hash_within_string(str);
        u64 key = string8_hashkey(hashStr);
        UIElement *res = ui_build_element_from_key(flags, key);
        res->text = ui_find_text_within_string(str);
        return res;
}

static UIElement *ui_build_element_from_stringf(UI_ElementFlags flags,
                                                char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        String8 str = string8fv(ui_frame_arena(), fmt, args);
        UIElement *res = ui_build_element_from_string(flags, str);
        va_end(args);
        return res;
}

static UIElement *ui_build_element_from_stringfv(UI_ElementFlags flags,
                                                 char *fmt, va_list args) {
        String8 str = string8fv(ui_frame_arena(), fmt, args);
        UIElement *res = ui_build_element_from_string(flags, str);
        return res;
}

/* UI autolayout algorithm functions */

static void ui_autolayout_calc_preorder(UIElement *e, UI_Axis2d axis) {
        UISemanticSize size = e->size[axis];
        f32 computedSize = 0.0f;
        f32 parentSize = e->parent ? e->parent->computedSize.v[axis] : 0.0f;

        switch (size.kind) {
        case UI_SizeKind_Pixels:
                computedSize = size.value;
                break;
        case UI_SizeKind_TextContent:
                if (axis == UI_Axis2d_X) {
                        computedSize = f_text_length(ui_state.defaultFont,
                                                     e->textSize, e->text);
                        computedSize += (e->padding.x + e->padding.z);
                        computedSize += (2 * e->borderSize);
                } else {
                        computedSize = e->textSize * ui_content_scale().y;
                        computedSize += (e->padding.y + e->padding.w);
                        computedSize += (2 * e->borderSize);
                }
                break;
        case UI_SizeKind_PercentOfParent:
                if (axis == UI_Axis2d_X) {
                        parentSize -= e->padding.x + e->padding.z;
                        computedSize = parentSize * size.value * 0.01f;
                } else {
                        parentSize -= e->padding.y + e->padding.w;
                        computedSize = parentSize * size.value * 0.01f;
                }
                break;
        case UI_SizeKind_SumOfChildren:
        case UI_SizeKind_OtherAxisRatio:
        case UI_SizeKind_Null:
                return;
        }

        e->computedSize.v[axis] = computedSize;
}

static void ui_autolayout_calc_postorder(UIElement *e, UI_Axis2d axis) {
        UISemanticSize size = e->size[axis];
        f32 computedSize = 0.0f;

        switch (size.kind) {
        case UI_SizeKind_SumOfChildren:
                for (UIElement *child = e->firstChild;
                     !ui_element_is_nil(child); child = child->next) {
                        if (axis == e->layoutDirection) {
                                computedSize += (axis == UI_Axis2d_X)
                                                    ? child->computedSize.x
                                                    : child->computedSize.y;
                        } else {
                                f32 childSize = (axis == UI_Axis2d_X)
                                                    ? child->computedSize.x
                                                    : child->computedSize.y;
                                computedSize = max(computedSize, childSize);
                        }
                }

                if (e->flags & UI_ElementFlag_DrawBorder) {
                        computedSize += (2 * e->borderSize);
                }
                if (axis == UI_Axis2d_X) {
                        computedSize += (e->padding.x + e->padding.z);
                        e->computedSize.x = computedSize;
                } else {
                        computedSize += (e->padding.y + e->padding.w);
                        e->computedSize.y = computedSize;
                }
                break;
        case UI_SizeKind_OtherAxisRatio:
                if (axis == UI_Axis2d_X) {
                        computedSize = e->computedSize.y;
                        computedSize -= (e->padding.y + e->padding.w);
                        computedSize *= size.value;
                        computedSize += (e->padding.x + e->padding.z);
                        e->computedSize.x = computedSize;
                } else {
                        computedSize = e->computedSize.x;
                        computedSize -= (e->padding.x + e->padding.z);
                        computedSize *= size.value;
                        computedSize += (e->padding.y + e->padding.w);
                        e->computedSize.y = computedSize;
                }
                break;
        case UI_SizeKind_Pixels:
        case UI_SizeKind_TextContent:
        case UI_SizeKind_PercentOfParent:
        case UI_SizeKind_Null:
                return;
        }
}

static void ui_autolayout_rec_postorder(UIElement *e) {
        if (!e) {
                return;
        }

        for (UIElement *child = e->firstChild; !ui_element_is_nil(child);
             child = child->next) {
                ui_autolayout_rec_postorder(child);
        }

        ui_autolayout_calc_postorder(e, UI_Axis2d_X);
        ui_autolayout_calc_postorder(e, UI_Axis2d_Y);
}

static void ui_solve_violations_on_axis(UIElement *e, UI_Axis2d axis) {
        f32 pad = axis == UI_Axis2d_X ? e->padding.x + e->padding.z
                                      : e->padding.y + e->padding.w;
        f32 cap = e->computedSize.v[axis] - pad;

        if (e->layoutDirection == axis) {
                f32 sum = 0.0f;
                f32 strictnessTotal = 0.0f;
                for (UIElement *ch = e->firstChild; !ui_element_is_nil(ch);
                     ch = ch->next) {
                        sum += ch->computedSize.v[axis];
                        strictnessTotal += (1.0f - ch->size[axis].strictness);
                }
                f32 discrp = sum - cap;
                if (discrp > 0 && !nequal_f32(strictnessTotal, 0.0f, 0.0001)) {
                        f32 f = discrp / strictnessTotal;
                        for (UIElement *ch = e->firstChild;
                             !ui_element_is_nil(ch); ch = ch->next) {
                                f32 r = f * (1 - ch->size[axis].strictness);
                                ch->computedSize.v[axis] -= r;
                        }
                }

        } else {
                for (UIElement *ch = e->firstChild; !ui_element_is_nil(ch);
                     ch = ch->next) {
                        if (ch->size[axis].strictness < 1.0f) {
                                ch->computedSize.v[axis]
                                    = min(ch->computedSize.v[axis], cap);
                        }
                }
        }
}

static void ui_element_autolayout(void) {
        Arena *scratch = ui_build_arena();
        u64 resetPos = arena_pos(scratch);
        Vec2f32 screenExtent = r_get_window_size();
        UIElement *root = ui_state.root;
        Vec4f32 bg = ui_state.backgroundColorStackBottom.v;
        root->backgroundColors[0] = bg;
        root->backgroundColors[1] = bg;
        root->backgroundColors[2] = bg;
        root->backgroundColors[3] = bg;
        // TODO: hr: hacky! need to fix
        root->size[UI_Axis2d_X].kind = UI_SizeKind_Pixels;
        root->size[UI_Axis2d_X].value = screenExtent.x;
        root->size[UI_Axis2d_Y].kind = UI_SizeKind_Pixels;
        root->size[UI_Axis2d_Y].value = screenExtent.y;

        UIElementList queue = { 0 };
        ui_element_list_push(scratch, &queue, root);
        while (!ui_element_list_is_empty(&queue)) {
                UIElement *e = ui_element_list_dequeue(&queue);
                UIElement *child = e->firstChild;

                ui_autolayout_calc_preorder(e, UI_Axis2d_X);
                ui_autolayout_calc_preorder(e, UI_Axis2d_Y);

                while (!ui_element_is_nil(child)) {
                        ui_element_list_push(scratch, &queue, child);
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

        arena_pop_at(scratch, resetPos);
        queue.first = 0;
        queue.last = 0;
        ui_element_list_push(scratch, &queue, root);
        while (!ui_element_list_is_empty(&queue)) {
                UIElement *cur = ui_element_list_dequeue(&queue);
                cur->lastFrameTouched = frame;

                // hr: solve violations
                ui_solve_violations_on_axis(cur, UI_Axis2d_X);
                ui_solve_violations_on_axis(cur, UI_Axis2d_Y);

                // hr: calculate relative positions and screen
                // coordinates
                for (UIElement *child = cur->firstChild;
                     !ui_element_is_nil(child); child = child->next) {
                        ui_element_list_push(scratch, &queue, child);
                        UIElement *prev = child->prev;
                        if (ui_element_is_nil(prev)
                            || cur->layoutDirection == UI_Axis2d_None) {
                                child->relPosition.x = 0.0f;
                                child->relPosition.y = 0.0f;
                        } else if (cur->layoutDirection == UI_Axis2d_X) {
                                child->relPosition.x = prev->relPosition.x
                                                       + prev->computedSize.x;
                                child->relPosition.y = 0.0f;
                        } else if (cur->layoutDirection == UI_Axis2d_Y) {
                                child->relPosition.x = 0.0f;
                                child->relPosition.y = prev->relPosition.y
                                                       + prev->computedSize.y;
                        }

                        Vec2f32 paddedParentPos
                            = add_v2f32(cur->screenCoords.p0,
                                        v2f32(cur->padding.x, cur->padding.y));
                        child->screenCoords.p0
                            = add_v2f32(paddedParentPos, child->relPosition);
                        child->screenCoords.p1 = add_v2f32(
                            child->computedSize, child->screenCoords.p0);
                }

                cur->texRange = rng2f32(v2f32(0, 0), v2f32(1, 1));
        }

        // hr: clipping
        ui_element_list_push(scratch, &queue, root);
        while (!ui_element_list_is_empty(&queue)) {
                UIElement *cur = ui_element_list_dequeue(&queue);

                for (UIElement *child = cur->firstChild;
                     !ui_element_is_nil(child); child = child->next) {
                        ui_element_list_push(scratch, &queue, child);

                        // NOTE: hr: does this work recursively? What if child
                        // has child that needs to be clipped? Furthurmore we
                        // need to extend what we are doing here if we want to
                        // clip text halfway through a character.

                        if (cur->flags & UI_ElementFlag_Clip) {
                                child->flags
                                    = child->flags | UI_ElementFlag_Clip;

                                Vec2f32 c0 = child->screenCoords.min;
                                Vec2f32 c1 = child->screenCoords.max;
                                Vec2f32 p0 = cur->screenCoords.min;
                                Vec2f32 p1 = cur->screenCoords.max;
                                if (p0.x > c0.x) {
                                        child->texRange.min.x
                                            = 1.0f
                                              - ((p1.x - p0.x) / (c1.x - c0.x));
                                        c0.x = p0.x;
                                }
                                if (p0.y > c0.y) {
                                        child->texRange.min.y
                                            = 1.0f
                                              - ((p1.y - p0.y) / (c1.y - c0.y));
                                        c0.y = p0.y;
                                }
                                if (p1.x < c1.x) {
                                        child->texRange.max.x
                                            = ((p1.x - p0.x) / (c1.x - c0.x));
                                        c1.x = p1.x;
                                }
                                if (p1.y < c1.y) {
                                        child->texRange.max.y
                                            = ((p1.y - p0.y) / (c1.y - c0.y));
                                        c1.y = p1.y;
                                }
                                child->screenCoords = rng2f32(c0, c1);
                        }
                }
        }

        arena_pop_at(scratch, resetPos);
}

static void ui_draw_element_rec(UIElement *e) {
        f32 border = e->borderSize;
        Rng2f32 pos = e->screenCoords;
        if (e->flags & UI_ElementFlag_DrawBorder) {
                Vec4f32 bc[4] = { e->borderColor, e->borderColor,
                                  e->borderColor, e->borderColor };
                dr_rect(pos, bc, 0, 0);
                pos.min = add_v2f32(pos.min, v2f32(border, border));
                pos.max = add_v2f32(pos.max, v2f32(-border, -border));
        }
        if (e->flags & UI_ElementFlag_DrawBackground) {
                if (e->texture) {
                        dr_img(pos, e->backgroundColors[0], e->texture,
                               e->texRange, e->cornerRadius, 0);
                } else {
                        dr_rect(pos, e->backgroundColors, e->cornerRadius, 0);
                }
        }
        if (e->flags & UI_ElementFlag_DrawText) {
                Vec4f32 p = e->padding;
                pos.min = add_v2f32(pos.min, v2f32(p.x, p.y));
                pos.max = add_v2f32(pos.max, v2f32(-p.z, -p.w));
                dr_text(ui_state.defaultFont, e->textSize, e->text, pos,
                        e->textColor);
        }

        for (UIElement *child = e->firstChild; !ui_element_is_nil(child);
             child = child->next) {
                ui_draw_element_rec(child);
        }
}

static void ui_draw_elements(void) {
        ui_signal_from_element(ui_state.root);

        ui_draw_element_rec(ui_state.root);

        r_dispatch_batch();

        u64 frame = r_get_frame_count();
        ui_element_cache_prune(frame);

        ui_state.root->firstChild = &ui_nil_element;
        ui_state.root->lastChild = &ui_nil_element;
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
        while (ui_state.textSizeStack.top != &ui_state.textSizeStackBottom) {
                ui_pop_text_size();
        }
        while (ui_state.backgroundColorStack.top
               != &ui_state.backgroundColorStackBottom) {
                ui_pop_background_color();
        }
        while (ui_state.borderSizeStack.top
               != &ui_state.borderSizeStackBottom) {
                ui_pop_border_size();
        }
        while (ui_state.borderColorStack.top
               != &ui_state.borderColorStackBottom) {
                ui_pop_border_color();
        }
        while (ui_state.paddingStack.top != &ui_state.paddingStackBottom) {
                ui_pop_padding();
        }
        while (ui_state.cornerRadiusStack.top
               != &ui_state.cornerRadiusStackBottom) {
                ui_pop_corner_radius();
        }

        // TODO: hr: these probably need to be moved into a function and
        // runs once at the end of every frame
        u32 left = glfwGetMouseButton(r_state.window, GLFW_MOUSE_BUTTON_LEFT);
        u32 middle
            = glfwGetMouseButton(r_state.window, GLFW_MOUSE_BUTTON_MIDDLE);
        u32 right = glfwGetMouseButton(r_state.window, GLFW_MOUSE_BUTTON_RIGHT);

        // WARN: hr: does this even work? (yes?) it's janky as hell
        if ((left == GLFW_RELEASE
             && ui_state.prevMouseState[UI_Button_Left] == GLFW_PRESS
             && !contains_r2f32(ui_state.focused->screenCoords, ui_mouse_pos()))
            || glfwGetKey(r_state.window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                ui_set_focused(&ui_nil_element, &string8_nil);
        }

        ui_state.prevMouseState[0] = left;
        ui_state.prevMouseState[1] = middle;
        ui_state.prevMouseState[2] = right;
        ui_state.prevMousePos = ui_mouse_pos();

        // TODO: hr: where should this go?
        arena_reset(ui_frame_arena());
}

static void ui_element_add_display_string(UIElement *e, String8 str) {
        e->text = str;
}

static void ui_element_attach_texture(UIElement *e, RTexture *tex) {
        e->texture = tex;
}

static void ui_element_add_child_layout_axis(UIElement *e, UI_Axis2d axis) {
        e->layoutDirection = axis;
}

static void ui_element_bg_colors(UIElement *e, Vec4f32 *colors) {
        memcpy(e->backgroundColors, colors, sizeof(Vec4f32) * 4);
}

static void ui_element_emboss_from_solid(UIElement *e) {
        Vec4f32 c = e->backgroundColors[0];
        f32 v = c.x + c.y + c.z;
        f32 a = (v > 1.5f) ? -0.2f : 0.2f;
        Vec4f32 nc
            = v4f32(clamp(0.0f, c.x + a, 1.0f), clamp(0.0f, c.y + a, 1.0f),
                    clamp(0.0f, c.x + a, 1.0f), c.w);
        e->backgroundColors[1] = nc;
        e->backgroundColors[3] = nc;
}

static void ui_element_flip_embossment(UIElement *e) {
        Vec4f32 tmp = e->backgroundColors[0];
        e->backgroundColors[0] = e->backgroundColors[1];
        e->backgroundColors[1] = tmp;
        tmp = e->backgroundColors[2];
        e->backgroundColors[2] = e->backgroundColors[3];
        e->backgroundColors[3] = tmp;
}

static inline UI_SignalFlags ui_mouse_signal(UI_Button button,
                                             UI_SignalFlags flags, UIElement *e,
                                             Vec2f32 mousePos, b32 inside) {
        u32 state = 0;
        if (button == UI_Button_Left) {
                state = glfwGetMouseButton(r_state.window,
                                           GLFW_MOUSE_BUTTON_LEFT);
        } else if (button == UI_Button_Middle) {
                state = glfwGetMouseButton(r_state.window,
                                           GLFW_MOUSE_BUTTON_MIDDLE);
        } else if (button == UI_Button_Right) {
                state = glfwGetMouseButton(r_state.window,
                                           GLFW_MOUSE_BUTTON_RIGHT);
        } else {
                return flags;
        }

        Rng2f32 bbox = e->screenCoords;
        u64 frame = r_get_frame_count();
        Vec2f32 origin = ui_state.pressOrigin[button];
        if (state == GLFW_PRESS) {
                if (inside) {
                        if (ui_state.prevMouseState[button] == GLFW_RELEASE
                            && origin.x < 0.0f) {
                                ui_state.pressOrigin[button] = mousePos;
                                ui_state.pressedElementKey[button] = e->key;
                                origin = mousePos;
                        }
                        flags |= (UI_SignalFlag_LeftPressed << button);
                }
                if (e->key == ui_state.pressedElementKey[button]
                    && !equal_v2f32(mousePos, origin)) {
                        flags |= (UI_SignalFlag_LeftDragging << button);
                }
        } else if (ui_state.prevMouseState[button] == GLFW_PRESS) {
                if (inside) {
                        if (contains_r2f32(bbox, origin)) {
                                if (contains_r2f32(bbox,
                                                   ui_state.prevClick[button])
                                    && (frame - ui_state.prevClickFrame[button])
                                           < 60) {
                                        flags
                                            |= (UI_SignalFlag_LeftDoubleClicked
                                                << button);
                                }
                                ui_state.prevClickFrame[button] = frame;
                                ui_state.prevClick[button]
                                    = ui_state.pressOrigin[button];
                                flags |= (UI_SignalFlag_LeftClicked << button);
                        }

                        flags |= (UI_SignalFlag_LeftReleased << button);

                        // hr: this probably needs to happen someplace
                        // else
                        ui_state.pressedElementKey[button] = 0;
                        ui_state.pressOrigin[button] = v2f32(-1, -1);
                }
        }

        return flags;
}

static UISignal ui_signal_from_element(UIElement *e) {
        UI_SignalFlags flags = 0;

        Vec2f32 mousePos = ui_mouse_pos();
        b32 inside = contains_r2f32(e->screenCoords, mousePos);

        // Vec2f32 n = v2f32(-1, -1);
        if (inside) {
                flags |= UI_SignalFlag_MouseOver;

                // TODO: hr: define the difference between hovering and
                // mousing over something. Is hovering when it's the
                // topmost element?

                // if ((equal_v2f32(ui_state.leftClickOrigin, n))
                //     && (equal_v2f32(ui_state.middleClickOrigin, n))
                //     && (equal_v2f32(ui_state.rightClickOrigin, n))) {
                flags |= UI_SignalFlag_Hovering;
                // }
        }

        for (u32 i = 0; i < UI_Button_Count; i++) {
                flags = ui_mouse_signal(i, flags, e, mousePos, inside);
        }

        // TODO: hr: there should be someway to mark ui events as
        // "eaten"

        UISignal sig = { .element = e, .flags = flags };
        return sig;
}
