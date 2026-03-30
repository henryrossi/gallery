// clang-format off
#define UIStackFuncImpl()\
static void ui_push_parent(UIElement * v) { UIStackPushImpl(ui_state, Parent, parent, v)}\
static UIElement * ui_top_parent(void) { UIStackTopImpl(ui_state, Parent, parent)}\
static UIElement * ui_pop_parent(void) { UIStackPopImpl(ui_state, Parent, parent)}\
static void ui_next_parent(UIElement * v) { UIStackSetNextImpl(ui_state, Parent, parent, v)}\
static void ui_push_text_size(f32 v) { UIStackPushImpl(ui_state, TextSize, textSize, v)}\
static f32 ui_top_text_size(void) { UIStackTopImpl(ui_state, TextSize, textSize)}\
static f32 ui_pop_text_size(void) { UIStackPopImpl(ui_state, TextSize, textSize)}\
static void ui_next_text_size(f32 v) { UIStackSetNextImpl(ui_state, TextSize, textSize, v)}\
static void ui_push_text_color(Vec4f32 v) { UIStackPushImpl(ui_state, TextColor, textColor, v)}\
static Vec4f32 ui_top_text_color(void) { UIStackTopImpl(ui_state, TextColor, textColor)}\
static Vec4f32 ui_pop_text_color(void) { UIStackPopImpl(ui_state, TextColor, textColor)}\
static void ui_next_text_color(Vec4f32 v) { UIStackSetNextImpl(ui_state, TextColor, textColor, v)}\
static void ui_push_background_color(Vec4f32 v) { UIStackPushImpl(ui_state, BackgroundColor, backgroundColor, v)}\
static Vec4f32 ui_top_background_color(void) { UIStackTopImpl(ui_state, BackgroundColor, backgroundColor)}\
static Vec4f32 ui_pop_background_color(void) { UIStackPopImpl(ui_state, BackgroundColor, backgroundColor)}\
static void ui_next_background_color(Vec4f32 v) { UIStackSetNextImpl(ui_state, BackgroundColor, backgroundColor, v)}\
static void ui_push_border_color(Vec4f32 v) { UIStackPushImpl(ui_state, BorderColor, borderColor, v)}\
static Vec4f32 ui_top_border_color(void) { UIStackTopImpl(ui_state, BorderColor, borderColor)}\
static Vec4f32 ui_pop_border_color(void) { UIStackPopImpl(ui_state, BorderColor, borderColor)}\
static void ui_next_border_color(Vec4f32 v) { UIStackSetNextImpl(ui_state, BorderColor, borderColor, v)}\
static void ui_push_border_size(f32 v) { UIStackPushImpl(ui_state, BorderSize, borderSize, v)}\
static f32 ui_top_border_size(void) { UIStackTopImpl(ui_state, BorderSize, borderSize)}\
static f32 ui_pop_border_size(void) { UIStackPopImpl(ui_state, BorderSize, borderSize)}\
static void ui_next_border_size(f32 v) { UIStackSetNextImpl(ui_state, BorderSize, borderSize, v)}\
static void ui_push_corner_radius(f32 v) { UIStackPushImpl(ui_state, CornerRadius, cornerRadius, v)}\
static f32 ui_top_corner_radius(void) { UIStackTopImpl(ui_state, CornerRadius, cornerRadius)}\
static f32 ui_pop_corner_radius(void) { UIStackPopImpl(ui_state, CornerRadius, cornerRadius)}\
static void ui_next_corner_radius(f32 v) { UIStackSetNextImpl(ui_state, CornerRadius, cornerRadius, v)}\
static void ui_push_padding(Vec4f32 v) { UIStackPushImpl(ui_state, Padding, padding, v)}\
static Vec4f32 ui_top_padding(void) { UIStackTopImpl(ui_state, Padding, padding)}\
static Vec4f32 ui_pop_padding(void) { UIStackPopImpl(ui_state, Padding, padding)}\
static void ui_next_padding(Vec4f32 v) { UIStackSetNextImpl(ui_state, Padding, padding, v)}\
static void ui_push_width(UISemanticSize v) { UIStackPushImpl(ui_state, Width, width, v)}\
static UISemanticSize ui_top_width(void) { UIStackTopImpl(ui_state, Width, width)}\
static UISemanticSize ui_pop_width(void) { UIStackPopImpl(ui_state, Width, width)}\
static void ui_next_width(UISemanticSize v) { UIStackSetNextImpl(ui_state, Width, width, v)}\
static void ui_push_height(UISemanticSize v) { UIStackPushImpl(ui_state, Height, height, v)}\
static UISemanticSize ui_top_height(void) { UIStackTopImpl(ui_state, Height, height)}\
static UISemanticSize ui_pop_height(void) { UIStackPopImpl(ui_state, Height, height)}\
static void ui_next_height(UISemanticSize v) { UIStackSetNextImpl(ui_state, Height, height, v)}\
