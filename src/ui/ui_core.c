#include "ui/generated/ui.c"

// NOTE: hr: for dropdowns, I think they should be removed from the main root
// tree, but still have a parent. They can be held in a seperate list and
// treated as their own boxes to be laid out

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

        for (u32 i = 0; i < UI_BUTTON_Count; i++) {
                ui_state.pressOrigin[i] = v2f32(-1.0f, -1.0f);
                ui_state.pressedElementKey[i] = 0;
        }

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
        ui_state.textSizeStackBottom.v = 16.0f;
        ui_state.textColorStackBottom.v = v4f32(0, 0, 0, 1);
        ui_state.backgroundColorStackBottom.v = v4f32(1, 1, 1, 1);
        ui_state.borderColorStackBottom.v = v4f32(0, 0, 0, 1);
        ui_state.borderSizeStackBottom.v = 1.0f;
        ui_state.cornerRadiusStackBottom.v = 0;
        ui_state.paddingStackBottom.v = v4f32(0, 0, 0, 0);
        ui_state.widthStackBottom.v
            = uiSemanticSize(UI_SIZEKIND_PercentOfParent, 100, 0);
        ui_state.heightStackBottom.v
            = uiSemanticSize(UI_SIZEKIND_PercentOfParent, 100, 0);

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

static UIElement *ui_cache_lookup(u64 key) {
        if (key == 0) {
                return 0;
        }
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
        res->layoutDirection = parent->layoutDirection; // NOTE: hr: this needs
                                                        // more thought
        res->size[UI_AXIS2D_X] = ui_top_width();
        res->size[UI_AXIS2D_Y] = ui_top_height();
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
                        computedSize += (e->padding.x + e->padding.z);
                        computedSize += (2 * e->borderSize);
                } else {
                        // TODO: hr: we need an algorithm that can
                        // compute how many lines of text a paragraph is
                        // given it's width.
                        computedSize = e->textSize;
                        computedSize += (e->padding.y + e->padding.w);
                        computedSize += (2 * e->borderSize);
                }
                break;
        case UI_SIZEKIND_PercentOfParent:
                parentSize = (axis == UI_AXIS2D_X) ? e->parent->computedSize.x
                                                   : e->parent->computedSize.y;
                computedSize = parentSize * size.value * 0.01f;
                break;
        case UI_SIZEKIND_SumOfChildren:
        case UI_SIZEKIND_Null:
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

        switch (size.kind) {
        case UI_SIZEKIND_SumOfChildren:
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

                computedSize += (2 * e->borderSize);
                if (axis == UI_AXIS2D_X) {
                        computedSize += (e->padding.x + e->padding.z);
                        e->computedSize.x = computedSize;
                } else {
                        computedSize += (e->padding.y + e->padding.w);
                        e->computedSize.y = computedSize;
                }
                break;

        case UI_SIZEKIND_Pixels:
        case UI_SIZEKIND_TextContent:
        case UI_SIZEKIND_PercentOfParent:
        case UI_SIZEKIND_Null:
                return;
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
        Vec4f32 bg = ui_state.backgroundColorStackBottom.v;
        root->backgroundColors[0] = bg;
        root->backgroundColors[1] = bg;
        root->backgroundColors[2] = bg;
        root->backgroundColors[3] = bg;
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
                        UIElement *prev = child->prev;
                        UIElement *parent = child->parent;
                        if (!child->prev
                            || parent->layoutDirection == UI_AXIS2D_None) {
                                child->relPosition.x = 0.0f;
                                child->relPosition.y = 0.0f;
                        } else if (child->parent->layoutDirection
                                   == UI_AXIS2D_X) {
                                child->relPosition.x = prev->relPosition.x
                                                       + prev->computedSize.x;
                        } else if (child->parent->layoutDirection
                                   == UI_AXIS2D_Y) {
                                child->relPosition.y = prev->relPosition.y
                                                       + prev->computedSize.y;
                        }

                        child->screenCoords.p0 = add_v2f32(
                            child->parent->screenCoords.p0, child->relPosition);
                        child->screenCoords.p1 = add_v2f32(
                            child->computedSize, child->screenCoords.p0);
                }
        }
}

static void ui_draw_element_rec(UIElement *e) {
        f32 border = e->borderSize;
        Rng2f32 pos = e->screenCoords;
        if (e->flags & UI_ELEMENTFLAG_DrawBorder) {
                Vec4f32 bc[4] = { e->borderColor, e->borderColor,
                                  e->borderColor, e->borderColor };
                dr_rect(pos, bc, 0, 0);
                pos.min = add_v2f32(pos.min, v2f32(border, border));
                pos.max = add_v2f32(pos.max, v2f32(-border, -border));
        }
        if (e->flags & UI_ELEMENTFLAG_DrawBackground) {
                if (e->texture) {
                        Rng2f32 src = r2f32p(0, 0, 1, 1);
                        dr_img(pos, e->backgroundColors[0], e->texture, src,
                               e->cornerRadius, 0);
                } else {
                        dr_rect(pos, e->backgroundColors, e->cornerRadius, 0);
                }
        }
        if (e->flags & UI_ELEMENTFLAG_DrawText) {
                Vec4f32 p = e->padding;
                pos.min = add_v2f32(pos.min, v2f32(p.x, p.y));
                pos.max = add_v2f32(pos.max, v2f32(-p.z, -p.w));
                dr_text(ui_state.defaultFont, e->textSize, e->text, pos,
                        e->textColor);
        }

        for (UIElement *child = e->firstChild; child; child = child->next) {
                ui_draw_element_rec(child);
        }
}

static void ui_draw_elements(void) {
        ui_signal_from_element(ui_state.root);

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

        // TODO: hr: these probably need to be moved into a function and
        // runs once at the end of every frame
        u32 left = glfwGetMouseButton(r_state.window, GLFW_MOUSE_BUTTON_LEFT);
        u32 middle
            = glfwGetMouseButton(r_state.window, GLFW_MOUSE_BUTTON_MIDDLE);
        u32 right = glfwGetMouseButton(r_state.window, GLFW_MOUSE_BUTTON_RIGHT);
        ui_state.prevMouseState[0] = left;
        ui_state.prevMouseState[1] = middle;
        ui_state.prevMouseState[2] = right;
        ui_state.prevMousePos = ui_mouse_pos();
}

static void ui_element_add_display_string(UIElement *e, String8 str) {
        e->text = str;
}

static void ui_element_attach_texture(UIElement *e, RTexture *tex) {
        e->texture = tex;
}

static void ui_element_add_child_layout_axis(UIElement *e, UI_AXIS2D axis) {
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

static inline UI_INTERACTIONFLAGS
ui_mouse_signal(UI_BUTTONS button, UI_INTERACTIONFLAGS flags, UIElement *e,
                Vec2f32 mousePos, b32 inside) {
        u32 state = 0;
        if (button == UI_BUTTON_Left) {
                state = glfwGetMouseButton(r_state.window,
                                           GLFW_MOUSE_BUTTON_LEFT);
        } else if (button == UI_BUTTON_Middle) {
                state = glfwGetMouseButton(r_state.window,
                                           GLFW_MOUSE_BUTTON_MIDDLE);
        } else if (button == UI_BUTTON_Right) {
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
                        flags |= (UI_INTERACTIONFLAG_LeftPressed << button);
                }
                if (e->key == ui_state.pressedElementKey[button]
                    && !equal_v2f32(mousePos, origin)) {
                        flags |= (UI_INTERACTIONFLAG_LeftDragging << button);
                }
        } else if (ui_state.prevMouseState[button] == GLFW_PRESS) {
                if (inside) {
                        if (contains_r2f32(bbox, origin)) {
                                if (contains_r2f32(bbox,
                                                   ui_state.prevClick[button])
                                    && (frame - ui_state.prevClickFrame[button])
                                           < 60) {
                                        flags
                                            |= (UI_INTERACTIONFLAG_LeftDoubleClicked
                                                << button);
                                }
                                ui_state.prevClickFrame[button] = frame;
                                ui_state.prevClick[button]
                                    = ui_state.pressOrigin[button];
                                flags |= (UI_INTERACTIONFLAG_LeftClicked
                                          << button);
                        }

                        flags |= (UI_INTERACTIONFLAG_LeftReleased << button);

                        // hr: this probably needs to happen someplace
                        // else
                        ui_state.pressedElementKey[button] = 0;
                        ui_state.pressOrigin[button] = v2f32(-1, -1);
                }
        }

        return flags;
}

static UISignal ui_signal_from_element(UIElement *e) {
        UI_INTERACTIONFLAGS flags = 0;

        Vec2f32 mousePos = ui_mouse_pos();
        b32 inside = contains_r2f32(e->screenCoords, mousePos);

        // Vec2f32 n = v2f32(-1, -1);
        if (inside) {
                flags |= UI_INTERACTIONFLAG_MouseOver;

                // TODO: hr: define the difference between hovering and
                // mousing over something. Is hovering when it's the
                // topmost element?

                // if ((equal_v2f32(ui_state.leftClickOrigin, n))
                //     && (equal_v2f32(ui_state.middleClickOrigin, n))
                //     && (equal_v2f32(ui_state.rightClickOrigin, n))) {
                flags |= UI_INTERACTIONFLAG_Hovering;
                // }
        }

        for (u32 i = 0; i < UI_BUTTON_Count; i++) {
                flags = ui_mouse_signal(i, flags, e, mousePos, inside);
        }

        // TODO: hr: there should be someway to mark ui events as
        // "eaten"

        UISignal sig = { .element = e, .flags = flags };
        return sig;
}
