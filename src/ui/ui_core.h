#ifndef UI_CORE_H
#define UI_CORE_H

typedef enum {
        UI_AXIS2D_X,
        UI_AXIS2D_Y,
        UI_AXIS2D_Count,
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
} SemanticSize;

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

        SemanticSize size[UI_AXIS2D_Count];
        Vec2 relPosition;
        Vec2 computedSize;
        Rect2D screenCoords;

        UI_AXIS2D layoutDirection;

        UI_ELEMENTFLAGS flags;
        String8 text;
        Vec4 backgroundColor;
        Vec4 textColor;
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
        UIInputFlagLeftPressed = (1 << 0),
        UIInputFlagMiddlePressed = (1 << 1),
        UIInputFlagRightPressed = (1 << 2),

        UIInputFlagLeftDragging = (1 << 3),
        UIInputFlagMiddleDragging = (1 << 4),
        UIInputFlagRightDragging = (1 << 5),

        UIInputFlagLeftReleased = (1 << 6),
        UIInputFlagMiddleReleased = (1 << 7),
        UIInputFlagRightReleased = (1 << 8),

        UIInputFlagLeftClicked = (1 << 9),
        UIInputFlagMiddleClicked = (1 << 10),
        UIInputFlagRightClicked = (1 << 11),

        UIInputFlagLeftDoubleClicked = (1 << 12),
        UIInputFlagMiddleDoubleClicked = (1 << 13),
        UIInputFlagRightDoubleClicked = (1 << 14),

        UIInputFlagKeyboardPressed = (1 << 15),

        UIInputFlagHovering = (1 << 16),
        UIInputFlagMouseOver = (1 << 17),
} UIInputFlags;

typedef struct {
        UIElement *element;
        UIInputFlags flags;
} UISignal;

typedef struct UITextColorNode UITextColorNode;
struct UITextColorNode {
        UITextColorNode *next;
        Vec4 v;
};

typedef struct UIBackgroundColorNode UIBackgroundColorNode;
struct UIBackgroundColorNode {
        UIBackgroundColorNode *next;
        Vec4 v;
};

typedef struct UIWidthNode UIWidthNode;
struct UIWidthNode {
        UIWidthNode *next;
        SemanticSize v;
};

typedef struct UIHeightNode UIHeightNode;
struct UIHeightNode {
        UIHeightNode *next;
        SemanticSize v;
};

typedef struct {
        Arena arena;

        u32 stackTop;
        u32 stackCount;
        UIElement **stack;

        UIElement *root;
        u32 numElements;

        u64 bucketCount;
        UIElement **buckets;

        UITextColorNode textColorStackBottom;
        UIBackgroundColorNode backgroundColorStackBottom;
        UIWidthNode widthStackBottom;
        UIHeightNode heightStackBottom;

        struct {
                UITextColorNode *top;
                UITextColorNode *free;
        } textColorStack;
        struct {
                UIBackgroundColorNode *top;
                UIBackgroundColorNode *free;
        } backgroundColorStack;
        struct {
                UIWidthNode *top;
                UIWidthNode *free;
        } widthStack;
        struct {
                UIHeightNode *top;
                UIHeightNode *free;
        } heightStack;
} UIState;

static Arena *ui_build_arena(void);

static SemanticSize semanticSize(UI_SIZEKIND kind, f32 value, f32 strictness);

static b32 ui_key_match(u64 a, u64 b);

static UIElement *ui_build_element_from_key(UI_ELEMENTFLAGS flags, u64 key);
static UIElement *ui_build_element_from_string(UI_ELEMENTFLAGS flags,
                                               String8 str);
static UIElement *ui_build_element_from_stringf(UI_ELEMENTFLAGS flags,
                                                char *fmt, ...);

static void ui_element_add_display_string(UIElement *e, String8 str);
static void ui_element_add_child_layout_axis(UIElement *e, UI_AXIS2D axis);

static void ui_push_parent(UIElement *e);
static UIElement *ui_get_top_parent(void);
static UIElement *ui_pop_parent(void);

static UISignal ui_signal_from_element(UIElement *e);

#define ui_stack_scope(begin, end)                                             \
        for (int _i_ = ((begin), 0); !_i_; _i_ += 1, (end))

static void ui_push_text_color(Vec4 color);
static Vec4 ui_get_top_text_color(void);
static Vec4 ui_pop_text_color(void);
#define ui_text_color(v)                                                       \
        ui_stack_scope(ui_push_text_color(v), ui_pop_text_color())

static void ui_push_background_color(Vec4 color);
static Vec4 ui_get_top_background_color(void);
static Vec4 ui_pop_background_color(void);
#define ui_background_color(v)                                                 \
        ui_stack_scope(ui_push_background_color(v), ui_pop_background_color())

static void ui_push_width(SemanticSize size);
static SemanticSize ui_get_top_width(void);
static SemanticSize ui_pop_width(void);
#define ui_width(s) ui_stack_scope(ui_push_width(s), ui_pop_width())

static void ui_push_height(SemanticSize size);
static SemanticSize ui_get_top_height(void);
static SemanticSize ui_pop_height(void);
#define ui_height(s) ui_stack_scope(ui_push_height(s), ui_pop_height())

#endif // UI_CORE_H
