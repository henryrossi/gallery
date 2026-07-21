#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include "ui/ui_core.h"

static void ui_spacer(UISemanticSize size);
static void ui_text(String8 text);

static UISignal ui_button(String8 text);
static UISignal ui_buttonf(char *fmt, ...);

static UISignal ui_textfield(String8 tag, String8 *text);

static UISignal ui_slider(f32 *val, Vec4f32 bgColor, String8 tag);
static UISignal ui_sliderf(f32 *val, Vec4f32 bgColor, char *fmt, ...);
static UISignal ui_checkbox(String8 tag);

static UISignal ui_hsv_color_picker(Vec4f32 *rgba, String8 tag);

#endif // UI_WIDGETS_H
