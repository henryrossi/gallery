#include "ui/ui_widgets.h"
#include "ui/ui_core.h"

static void ui_spacer(UISemanticSize size) {
        String8 nstr = { 0 };
        UIElement *e = ui_build_element_from_string(0, nstr);
        e->size[e->layoutDirection] = size;
}

static void ui_text(String8 text) {
        UIElement *e = ui_build_element_from_string(UI_ElementFlag_DrawText,
                                                    string8_empty());
        ui_element_add_display_string(e, text);
}

static UISignal ui_button(String8 text) {
        ui_push_width(uiSizeTextContent(1));
        ui_push_height(uiSizeTextContent(1));
        // this needs to be based on text size
        ui_push_padding(v4f32(8, 4, 8, 4));

        UIElement *e = ui_build_element_from_string(
            UI_ElementFlag_Clickable | UI_ElementFlag_DrawText
                | UI_ElementFlag_DrawBorder | UI_ElementFlag_DrawBackground,
            text);
        UISignal sig = ui_signal_from_element(e);

        // ui_element_emboss_from_solid(e);
        // if (sig.flags & UI_SignalFlag_LeftPressed) {
        //         ui_element_flip_embossment(e);
        // }

        ui_pop_width();
        ui_pop_height();
        ui_pop_padding();

        return sig;
}

static UISignal ui_buttonf(char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        // TODO: hr: replace this with string8f and remove
        // ui_build_element_from_stringfv as a function
        UIElement *e = ui_build_element_from_stringfv(0, fmt, args);
        UISignal sig = ui_signal_from_element(e);
        va_end(args);
        return sig;
}

static UISignal ui_slider(f32 *val, String8 text) {
        f32 SLIDER_HEIGHT = ui_scale_value(ui_top_text_size(), UI_Axis2d_Y);
        f32 SLIDER_WIDTH = SLIDER_HEIGHT * 5.0f;
        f32 HALF_SLIDER_HEIGHT = SLIDER_HEIGHT / 2.0f;
        String8 nstr = { 0 };
        f32 v = clamp(0.0f, *val, 1.0f);

        ui_push_width(uiPixels(SLIDER_WIDTH, 1));
        ui_push_height(uiPixels(SLIDER_HEIGHT, 1));

        UIElement *a = ui_build_element_from_string(0, nstr);
        ui_push_parent(a);
        a->layoutDirection = UI_Axis2d_None;

        UIElement *b = ui_build_element_from_string(0, nstr);
        b->layoutDirection = UI_Axis2d_Y;
        UIElement *bar = 0;
        ui_parent(b) {
                ui_spacer(uiPct(25, 1));

                bar = ui_build_element_from_string(
                    UI_ElementFlag_DrawBackground,
                    string8_concat(ui_state.strArena, text,
                                   string8_lit("bar")));
                bar->size[UI_Axis2d_Y] = uiPct(50, 1);
                bar->cornerRadius = HALF_SLIDER_HEIGHT / 2.0f;
        }

        UIElement *c = ui_build_element_from_string(0, nstr);
        c->layoutDirection = UI_Axis2d_X;
        UISignal sig = { 0 };
        ui_parent(c) {
                ui_spacer(uiPixels(v * (SLIDER_WIDTH - SLIDER_HEIGHT), 1));

                ui_push_background_color(ui_top_text_color());
                UIElement *nob = ui_build_element_from_string(
                    UI_ElementFlag_Clickable | UI_ElementFlag_DrawBackground,
                    text);
                nob->size[UI_Axis2d_X] = uiPixels(SLIDER_HEIGHT, 1);
                nob->cornerRadius = HALF_SLIDER_HEIGHT;
                ui_pop_background_color();

                sig = ui_signal_from_element(nob);
                if (ui_dragging(sig)) {
                        Rng2f32 rng = bar->screenCoords;
                        Vec2f32 mousePos = ui_mouse_pos();
                        f32 v = (mousePos.x - rng.min.x - HALF_SLIDER_HEIGHT)
                                / (rng.max.x - rng.min.x - SLIDER_HEIGHT);
                        *val = clamp(0.0f, v, 1.0f);
                }
        }

        ui_pop_parent();
        ui_pop_width();
        ui_pop_height();
        return sig;
}

static UISignal ui_hsv_color_picker(String8 text) {
        f32 fs = ui_top_text_size();

        ui_next_width(uiSizeSumOfChildren(1));
        ui_next_height(uiSizeSumOfChildren(1));
        UIElement *e = ui_build_element_from_string(0, string8_empty());
        e->layoutDirection = UI_Axis2d_X;
        ui_push_parent(e);

        ui_push_width(uiPixelsX(4 * fs, 1));
        ui_push_height(uiPixelsY(4 * fs, 1));
        ui_next_corner_radius(2 * fs);
        e = ui_build_element_from_string(UI_ElementFlag_DrawBackground
                                             | UI_ElementFlag_Clickable,
                                         string8_lit("circle"));
        Vec4f32 r = v4f32(1, 0, 0, 1);
        Vec4f32 g = v4f32(0, 1, 0, 1);
        Vec4f32 b = v4f32(0, 0, 1, 1);
        e->backgroundColors[0] = r;
        e->backgroundColors[1] = r;
        e->backgroundColors[2] = g;
        e->backgroundColors[3] = b;

        ui_pop_parent();
}
