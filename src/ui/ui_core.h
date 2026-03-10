#ifndef UI_CORE_H
#define UI_CORE_H

#include "generated/ui.h"

typedef enum {
        UI_AXIS2D_X,
        UI_AXIS2D_Y,
        UI_AXIS2D_Count,
        UI_AXIS2D_None,
} UI_AXIS2D;

typedef enum {
        UI_SIZEKIND_Null,
        UI_SIZEKIND_Pixels,
        UI_SIZEKIND_TextContent,
        UI_SIZEKIND_PercentOfParent,
        UI_SIZEKIND_SumOfChildren,
} UI_SIZEKIND;

typedef struct {
        UI_SIZEKIND kind;
        f32 value;
        f32 strictness;
} UISemanticSize;

typedef enum {
        UI_ELEMENTFLAG_Clickable = (1 << 0),
        UI_ELEMENTFLAG_ViewScroll = (1 << 1),
        UI_ELEMENTFLAG_DrawText = (1 << 2),
        UI_ELEMENTFLAG_DrawBorder = (1 << 3),
        UI_ELEMENTFLAG_DrawBackground = (1 << 4),
        UI_ELEMENTFLAG_DrawDropShadow = (1 << 5),
        UI_ELEMENTFLAG_Clip = (1 << 6),
        UI_ELEMENTFLAG_HotAnimation = (1 << 7),
        UI_ELEMENTFLAG_ActiveAnimation = (1 << 8),
} UI_ELEMENTFLAGS;

typedef struct UIElement UIElement;
struct UIElement {
        UIElement *parent;
        UIElement *next;
        UIElement *prev;
        UIElement *firstChild;
        UIElement *lastChild;

        UIElement *hashNext;
        UIElement *hashPrev;
        u64 key;
        u64 lastFrameTouched;

        UI_ELEMENTFLAGS flags;
        UISemanticSize size[UI_AXIS2D_Count];
        UI_AXIS2D layoutDirection;
        String8 text;
        f32 textSize;
        Vec4f32 textColor;
        Vec4f32 backgroundColors[4]; // embossing? gradient?
        Vec4f32 borderColor;
        f32 borderSize;
        f32 cornerRadius;
        Vec4f32 padding;
        RTexture *texture;

        // hr: autolayout computed
        Vec2f32 relPosition;
        Vec2f32 computedSize;
        Rng2f32 screenCoords;
};

typedef struct UIElementNode UIElementNode;
struct UIElementNode {
        UIElement *element;
        UIElementNode *next;
};

typedef struct UIElementList UIElementList;
struct UIElementList {
        UIElementNode *first;
        UIElementNode *last;
};

static void ui_element_list_append(Arena *arena, UIElementList *list,
                                   UIElement *element);
static UIElement *ui_element_list_pop_first(UIElementList *list);
static UIElement *ui_element_list_pop_last(UIElementList *list);

typedef enum {
        UI_INTERACTIONFLAG_LeftPressed = (1 << 0),
        UI_INTERACTIONFLAG_MiddlePressed = (1 << 1),
        UI_INTERACTIONFLAG_RightPressed = (1 << 2),

        UI_INTERACTIONFLAG_LeftDragging = (1 << 3),
        UI_INTERACTIONFLAG_MiddleDragging = (1 << 4),
        UI_INTERACTIONFLAG_RightDragging = (1 << 5),

        UI_INTERACTIONFLAG_LeftReleased = (1 << 6),
        UI_INTERACTIONFLAG_MiddleReleased = (1 << 7),
        UI_INTERACTIONFLAG_RightReleased = (1 << 8),

        UI_INTERACTIONFLAG_LeftClicked = (1 << 9),
        UI_INTERACTIONFLAG_MiddleClicked = (1 << 10),
        UI_INTERACTIONFLAG_RightClicked = (1 << 11),

        UI_INTERACTIONFLAG_LeftDoubleClicked = (1 << 12),
        UI_INTERACTIONFLAG_MiddleDoubleClicked = (1 << 13),
        UI_INTERACTIONFLAG_RightDoubleClicked = (1 << 14),

        UI_INTERACTIONFLAG_KeyboardPressed = (1 << 15),

        UI_INTERACTIONFLAG_Hovering = (1 << 16),
        UI_INTERACTIONFLAG_MouseOver = (1 << 17),

} UI_INTERACTIONFLAGS;

typedef enum {
        UI_BUTTON_Left,
        UI_BUTTON_Middle,
        UI_BUTTON_Right,
        UI_BUTTON_Count,
} UI_BUTTONS;

typedef struct {
        UIElement *element;
        UI_INTERACTIONFLAGS flags;
} UISignal;

// clang-format off
UIStackNodesDecl

typedef struct {
        Arena *arena;
        Arena *strArena;

        FFont *defaultFont;

        UIElement *root;
        u32 numElements;

        u64 bucketCount;
        UIElement **buckets;
        UIElement *eFree;

        Vec2f32 prevMousePos;
        u32 prevMouseState[UI_BUTTON_Count];
        Vec2f32 pressOrigin[UI_BUTTON_Count];
        u64 pressedElementKey[UI_BUTTON_Count];
        u64 prevClickFrame[UI_BUTTON_Count];
        Vec2f32 prevClick[UI_BUTTON_Count];

        UIStacksDecl
} UIState;
// clang-format on

static Arena *ui_build_arena(void);

static UISemanticSize uiSemanticSize(UI_SIZEKIND kind, f32 value,
                                     f32 strictness);
#define uiPixels(p, s) uiSemanticSize(UI_SIZEKIND_Pixels, p, s)
#define uiPct(p, s) uiSemanticSize(UI_SIZEKIND_PercentOfParent, p, s)
#define uiSizeSumOfChildren(s) uiSemanticSize(UI_SIZEKIND_SumOfChildren, 0, s)
#define uiSizeTextContent(s) uiSemanticSize(UI_SIZEKIND_TextContent, 0, s)

static b32 ui_key_match(u64 a, u64 b);

static UIElement *ui_build_element_from_key(UI_ELEMENTFLAGS flags, u64 key);
static UIElement *ui_build_element_from_string(UI_ELEMENTFLAGS flags,
                                               String8 str);
static UIElement *ui_build_element_from_stringf(UI_ELEMENTFLAGS flags,
                                                char *fmt, ...);
static UIElement *ui_build_element_from_stringfv(UI_ELEMENTFLAGS flags,
                                                 char *fmt, va_list args);

static void ui_element_add_display_string(UIElement *e, String8 str);
static void ui_element_add_child_layout_axis(UIElement *e, UI_AXIS2D axis);
static void ui_element_bg_colors(UIElement *e, Vec4f32 *colors);

static UISignal ui_signal_from_element(UIElement *e);

#define ui_stack_scope(begin, end)                                             \
        for (int _i_ = ((begin), 0); !_i_; _i_ += 1, (end))

static void ui_push_parent(UIElement *v);
static UIElement *ui_top_parent(void);
static UIElement *ui_pop_parent(void);

static void ui_push_text_size(f32 v);
static Vec4f32 ui_top_size_color(void);
static Vec4f32 ui_pop_text_color(void);

static void ui_push_text_color(Vec4f32 v);
static Vec4f32 ui_top_text_color(void);
static Vec4f32 ui_pop_text_color(void);

static void ui_push_background_color(Vec4f32 v);
static Vec4f32 ui_top_background_color(void);
static Vec4f32 ui_pop_background_color(void);

static void ui_push_width(UISemanticSize v);
static UISemanticSize ui_top_width(void);
static UISemanticSize ui_pop_width(void);

static void ui_push_height(UISemanticSize v);
static UISemanticSize ui_top_height(void);
static UISemanticSize ui_pop_height(void);

#define ui_parent(v) ui_stack_scope(ui_push_parent(v), ui_pop_parent())
#define ui_text_size(v) ui_stack_scope(ui_push_text_size(v), ui_pop_text_size())
#define ui_text_color(v)                                                       \
        ui_stack_scope(ui_push_text_color(v), ui_pop_text_color())
#define ui_background_color(v)                                                 \
        ui_stack_scope(ui_push_background_color(v), ui_pop_background_color())
#define ui_width(v) ui_stack_scope(ui_push_width(v), ui_pop_width())
#define ui_height(v) ui_stack_scope(ui_push_height(v), ui_pop_height())

#endif // UI_CORE_H
