// clang-format off
#define UIStackFuncImpl()\
static void ui_push_parent(UIElement * v) { UIStackPushImpl(ui_state, Parent, parent, v)}\
static UIElement * ui_top_parent(void) { UIStackTopImpl(ui_state, Parent, parent)}\
static UIElement * ui_pop_parent(void) { UIStackPopImpl(ui_state, Parent, parent)}\
static void ui_push_text_size(f32 v) { UIStackPushImpl(ui_state, TextSize, textSize, v)}\
static f32 ui_top_text_size(void) { UIStackTopImpl(ui_state, TextSize, textSize)}\
static f32 ui_pop_text_size(void) { UIStackPopImpl(ui_state, TextSize, textSize)}\
static void ui_push_text_color(Vec4f32 v) { UIStackPushImpl(ui_state, TextColor, textColor, v)}\
static Vec4f32 ui_top_text_color(void) { UIStackTopImpl(ui_state, TextColor, textColor)}\
static Vec4f32 ui_pop_text_color(void) { UIStackPopImpl(ui_state, TextColor, textColor)}\
static void ui_push_background_color(Vec4f32 v) { UIStackPushImpl(ui_state, BackgroundColor, backgroundColor, v)}\
static Vec4f32 ui_top_background_color(void) { UIStackTopImpl(ui_state, BackgroundColor, backgroundColor)}\
static Vec4f32 ui_pop_background_color(void) { UIStackPopImpl(ui_state, BackgroundColor, backgroundColor)}\
static void ui_push_width(UISemanticSize v) { UIStackPushImpl(ui_state, Width, width, v)}\
static UISemanticSize ui_top_width(void) { UIStackTopImpl(ui_state, Width, width)}\
static UISemanticSize ui_pop_width(void) { UIStackPopImpl(ui_state, Width, width)}\
static void ui_push_height(UISemanticSize v) { UIStackPushImpl(ui_state, Height, height, v)}\
static UISemanticSize ui_top_height(void) { UIStackTopImpl(ui_state, Height, height)}\
static UISemanticSize ui_pop_height(void) { UIStackPopImpl(ui_state, Height, height)}\

