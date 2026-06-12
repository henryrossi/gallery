#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include "ui/ui_core.h"

static void ui_spacer(UISemanticSize size);
static UISignal ui_button(String8 text);
static UISignal ui_buttonf(char *fmt, ...);
static UISignal ui_slider(f32 *val, Vec4f32 bgColor, String8 text);
static UISignal ui_sliderf(f32 *val, Vec4f32 bgColor, char *fmt, ...);
static UISignal ui_checkbox(void);
static UISignal ui_hsv_color_picker(Vec4f32 *rgba, String8 text);

#endif // UI_WIDGETS_H
