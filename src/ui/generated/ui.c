#define UIStackFuncImpl()\
static void ui_push_parent(UIElement * v) { UIStackPushImpl(ui_state, Parent, parent, v)}\
static UIElement * ui_top_parent(void) { UIStackTopImpl(ui_state, Parent, parent)}\
static UIElement * ui_pop_parent(void) { UIStackPopImpl(ui_state, Parent, parent)}\
static void ui_push_text_color(Vec4 v) { UIStackPushImpl(ui_state, TextColor, textColor, v)}\
static Vec4 ui_top_text_color(void) { UIStackTopImpl(ui_state, TextColor, textColor)}\
static Vec4 ui_pop_text_color(void) { UIStackPopImpl(ui_state, TextColor, textColor)}\
static void ui_push_background_color(Vec4 v) { UIStackPushImpl(ui_state, BackgroundColor, backgroundColor, v)}\
static Vec4 ui_top_background_color(void) { UIStackTopImpl(ui_state, BackgroundColor, backgroundColor)}\
static Vec4 ui_pop_background_color(void) { UIStackPopImpl(ui_state, BackgroundColor, backgroundColor)}\
static void ui_push_width(SemanticSize v) { UIStackPushImpl(ui_state, Width, width, v)}\
static SemanticSize ui_top_width(void) { UIStackTopImpl(ui_state, Width, width)}\
static SemanticSize ui_pop_width(void) { UIStackPopImpl(ui_state, Width, width)}\
static void ui_push_heigth(SemanticSize v) { UIStackPushImpl(ui_state, Height, height, v)}\
static SemanticSize ui_top_heigth(void) { UIStackTopImpl(ui_state, Height, height)}\
static SemanticSize ui_pop_heigth(void) { UIStackPopImpl(ui_state, Height, height)}\

