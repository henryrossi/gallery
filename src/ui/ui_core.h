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

#endif // UI_CORE_H
