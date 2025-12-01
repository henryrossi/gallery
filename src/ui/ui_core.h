#ifndef UI_CORE_H
#define UI_CORE_H

typedef enum {
        AXIS2D_X,
        AXIS2D_Y,
        AXIS2D_Count,
} Axis2D;

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
        UI_ELEMENTFLAG_Clickable = (1<<0),
        UI_ELEMENTFLAG_ViewScroll = (1<<1),
        UI_ELEMENTFLAG_DrawText = (1<<2),
        UI_ELEMENTFLAG_DrawBorder = (1<<3),
        UI_ELEMENTFLAG_DrawBackground = (1<<4),
        UI_ELEMENTFLAG_DrawDropShadow = (1<<5),
        UI_ELEMENTFLAG_Clip = (1<<6),
        UI_ELEMENTFLAG_HotAnimation = (1<<7),
        UI_ELEMENTFLAG_ActiveAnimation = (1<<8),
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

        SemanticSize size[AXIS2D_Count];
        Vec2 relPosition;
        Vec2 computedSize;
        Rect2D screenCoords;

        Axis2D layoutDirection;        

        UI_ELEMENTFLAGS flags;
        String8 text;
        Vec4 backgroundColor;
        Vec4 textColor;
};

typedef struct UIElementNode UIElementNode;
struct UIElementNode{
        UIElement *element;
        UIElementNode *next;
};

typedef struct UIElementList UIElementList;
struct UIElementList{
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

typedef struct {
        Arena arena;
        
        u32 stackTop;
        u32 stackCount;
        UIElement **stack;

        UIElement *root;
        u32 numElements;

        u64 bucketCount;
        UIElement **buckets;
} UIState;


static u64 ui_key_from_string(String8 string);
static b32 ui_key_match(u64 a, u64 b);

static UIElement *ui_make_element(UI_ELEMENTFLAGS flags, String8 str);
static UIElement *ui_make_elementf(UI_ELEMENTFLAGS flags, char *fmt, ...);

static void ui_element_add_display_string(UIElement *e, String8 str);
static void ui_element_add_child_layout_axis(UIElement *e, AXIS2D axis);

static void ui_push_parent(UIElement *e);
static UIElement *ui_get_top_parent(void);
static UIElement *ui_pop_parent(void);

static UISignal ui_signal_from_element(UIElement *e);

#endif // UI_CORE_H
