#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

static UISignal ui_button(String8 text);
static UISignal ui_buttonf(char *fmt, ...);

static UISignal ui_slider();
static UISignal ui_checkbox();
static UISignal ui_menu();
#endif // UI_WIDGETS_H
