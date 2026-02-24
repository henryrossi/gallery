#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

static void ui_spacer(UISemanticSize size);
static UISignal ui_button(String8 text);
static UISignal ui_buttonf(char *fmt, ...);
static UISignal ui_slider(f32 *val, String8 text);
static UISignal ui_sliderf(f32 *val, char *fmt, ...);
static UISignal ui_checkbox();

#endif // UI_WIDGETS_H
