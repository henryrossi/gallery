#ifndef UI_CORE_H
#define UI_CORE_H

#include "bedrock/bedrock_inc.h"
#include "font/font.h"
#include "render/render_inc.h"

#include "generated/ui.h"

typedef enum {
        UI_Axis2d_X,
        UI_Axis2d_Y,
        UI_Axis2d_Count,
        UI_Axis2d_None,
} UI_Axis2d;

typedef enum {
        UI_SizeKind_Null,
        UI_SizeKind_Pixels,
        UI_SizeKind_TextContent,
        UI_SizeKind_PercentOfParent,
        UI_SizeKind_SumOfChildren,
        UI_SizeKind_OtherAxisRatio,
} UI_SizeKind;

typedef struct {
        UI_SizeKind kind;
        f32 value;
        f32 strictness;
} UISemanticSize;

typedef enum {
        UI_ElementFlag_Clickable = (1 << 0),
        UI_ElementFlag_ViewScroll = (1 << 1),
        UI_ElementFlag_DrawText = (1 << 2),
        UI_ElementFlag_DrawBorder = (1 << 3),
        UI_ElementFlag_DrawBackground = (1 << 4),
        UI_ElementFlag_DrawDropShadow = (1 << 5),
        UI_ElementFlag_Clip = (1 << 6),
        UI_ElementFlag_HotAnimation = (1 << 7),
        UI_ElementFlag_ActiveAnimation = (1 << 8),
} UI_ElementFlags;

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

        UI_ElementFlags flags;
        UISemanticSize size[UI_Axis2d_Count];
        UI_Axis2d layoutDirection;
        String8 text;
        f32 textSize;
        Vec4f32 textColor;
        Vec4f32 backgroundColors[4];
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
        UI_SignalFlag_LeftPressed = (1 << 0),
        UI_SignalFlag_MiddlePressed = (1 << 1),
        UI_SignalFlag_RightPressed = (1 << 2),

        UI_SignalFlag_LeftDragging = (1 << 3),
        UI_SignalFlag_MiddleDragging = (1 << 4),
        UI_SignalFlag_RightDragging = (1 << 5),

        UI_SignalFlag_LeftReleased = (1 << 6),
        UI_SignalFlag_MiddleReleased = (1 << 7),
        UI_SignalFlag_RightReleased = (1 << 8),

        UI_SignalFlag_LeftClicked = (1 << 9),
        UI_SignalFlag_MiddleClicked = (1 << 10),
        UI_SignalFlag_RightClicked = (1 << 11),

        UI_SignalFlag_LeftDoubleClicked = (1 << 12),
        UI_SignalFlag_MiddleDoubleClicked = (1 << 13),
        UI_SignalFlag_RightDoubleClicked = (1 << 14),

        UI_SignalFlag_KeyboardPressed = (1 << 15),

        UI_SignalFlag_Hovering = (1 << 16),
        UI_SignalFlag_MouseOver = (1 << 17),

        UI_SignalFlag_Pressed
        = UI_SignalFlag_LeftPressed | UI_SignalFlag_KeyboardPressed,
        UI_SignalFlag_Released = UI_SignalFlag_LeftReleased,
        UI_SignalFlag_Clicked
        = UI_SignalFlag_LeftClicked | UI_SignalFlag_KeyboardPressed,
        UI_SignalFlag_DoubleClicked = UI_SignalFlag_LeftDoubleClicked,
        UI_SignalFlag_Dragging = UI_SignalFlag_LeftDragging,
} UI_SignalFlags;

#define ui_pressed(s) !!((s).flags & UI_SignalFlag_Pressed)
#define ui_released(s) !!((s).flags & UI_SignalFlag_Released)
#define ui_clicked(s) !!((s).flags & UI_SignalFlag_Clicked)
#define ui_double_clicked(s) !!((s).flags & UI_SignalFlag_DoubleClicked)
#define ui_middle_clicked(s) !!((s).flags & UI_SignalFlag_MiddleClicked)
#define ui_right_clicked(s) !!((s).flags & UI_SignalFlag_RightClicked)
#define ui_dragging(s) !!((s).flags & UI_SignalFlag_Dragging)
#define ui_hovering(s) !!((s).flags & UI_SignalFlag_Hovering)
#define ui_mouse_over(s) !!((s).flags & UI_SignalFlag_MouseOver)

typedef enum {
        UI_Button_Left,
        UI_Button_Middle,
        UI_Button_Right,
        UI_Button_Count,
} UI_Button;

typedef struct {
        UIElement *element;
        UI_SignalFlags flags;
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
        u32 prevMouseState[UI_Button_Count];
        Vec2f32 pressOrigin[UI_Button_Count];
        u64 pressedElementKey[UI_Button_Count];
        u64 prevClickFrame[UI_Button_Count];
        Vec2f32 prevClick[UI_Button_Count];

        UIStacksDecl
} UIState;
// clang-format on

static Arena *ui_build_arena(void);

static UISemanticSize uiSemanticSize(UI_SizeKind kind, f32 value,
                                     f32 strictness);
static f32 ui_scale_value(f32 value, UI_Axis2d scaledBy);
#define uiPixels(p, s) uiSemanticSize(UI_SizeKind_Pixels, p, s)
#define uiPixelsX(p, s)                                                        \
        uiSemanticSize(UI_SizeKind_Pixels, ui_scale_value(p, UI_Axis2d_X), s)
#define uiPixelsY(p, s)                                                        \
        uiSemanticSize(UI_SizeKind_Pixels, ui_scale_value(p, UI_Axis2d_Y), s)
#define uiPct(p, s) uiSemanticSize(UI_SizeKind_PercentOfParent, p, s)
#define uiRatio(r, s) uiSemanticSize(UI_SizeKind_OtherAxisRatio, r, s)
#define uiSizeSumOfChildren(s) uiSemanticSize(UI_SizeKind_SumOfChildren, 0, s)
#define uiSizeTextContent(s) uiSemanticSize(UI_SizeKind_TextContent, 0, s)

static b32 ui_key_match(u64 a, u64 b);

// read_only global UI_Box ui_nil_box = {
//         &ui_nil_box, &ui_nil_box, &ui_nil_box, &ui_nil_box,
//         &ui_nil_box, &ui_nil_box, &ui_nil_box,
// };

static UIElement *ui_build_element_from_key(UI_ElementFlags flags, u64 key);
static UIElement *ui_build_element_from_string(UI_ElementFlags flags,
                                               String8 str);
static UIElement *ui_build_element_from_stringf(UI_ElementFlags flags,
                                                char *fmt, ...);
static UIElement *ui_build_element_from_stringfv(UI_ElementFlags flags,
                                                 char *fmt, va_list args);

static void ui_element_add_display_string(UIElement *e, String8 str);
static void ui_element_add_child_layout_axis(UIElement *e, UI_Axis2d axis);
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
