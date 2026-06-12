#include "ui/ui_widgets.h"
#include "os/os.h"
#include "ui/ui_core.h"

static void ui_spacer(UISemanticSize size) {
        String8 nstr = { 0 };
        UIElement *e = ui_build_element_from_string(0, nstr);
        e->size[e->layoutDirection] = size;
}

static void ui_text(String8 text) {
        ui_next_width(uiSizeTextContent(1));
        ui_next_height(uiSizeTextContent(1));
        UIElement *e = ui_build_element_from_string(UI_ElementFlag_DrawText,
                                                    string8_empty());
        ui_element_add_display_string(e, text);
}

static UISignal ui_button(String8 text) {
        ui_push_width(uiSizeTextContent(1));
        ui_push_height(uiSizeTextContent(1));
        // TODO: hr: this needs to be based on text size
        ui_push_padding(v4f32(8, 4, 8, 4));

        UIElement *e = ui_build_element_from_string(
            UI_ElementFlag_Clickable | UI_ElementFlag_DrawText
                | UI_ElementFlag_DrawBorder | UI_ElementFlag_DrawBackground,
            text);
        UISignal sig = ui_signal_from_element(e);

        // ui_element_emboss_from_solid(e);
        if (sig.flags & UI_SignalFlag_LeftPressed) {
                ui_element_emboss_from_solid(e);
                // ui_element_flip_embossment(e);
        }

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

static Vec4f32 ui_hsv_to_rgb(Vec4f32 hsv) {
        f32 thirdPi = M_PI / 3.0f;

        f32 C = hsv.z * hsv.y;
        f32 X = C * (1.0f - fabsf(fmodf(hsv.x / thirdPi, 2.0f) - 1.0f));
        f32 m = hsv.z - C;
        Vec4f32 rgb = { 0 };

        if (hsv.x < 0.0f || hsv.x >= 2.0f * M_PI) {
                fprintf(stderr, "Hue outside of range");
                os_abort(1);
        }
        if (hsv.x < thirdPi) {
                rgb = v4f32(C, X, 0.0f, 1.0f);
        } else if (hsv.x < 2.0f * thirdPi) {
                rgb = v4f32(X, C, 0.0f, 1.0f);
        } else if (hsv.x < 3.0f * thirdPi) {
                rgb = v4f32(0.0f, C, X, 1.0f);
        } else if (hsv.x < 4.0f * thirdPi) {
                rgb = v4f32(0.0f, X, C, 1.0f);
        } else if (hsv.x < 5.0f * thirdPi) {
                rgb = v4f32(X, 0.0f, C, 1.0f);
        } else {
                rgb = v4f32(C, 0.0f, X, 1.0f);
        }

        return add_v4f32(rgb, v4f32(m, m, m, 0.0f));
}

static Vec4f32 ui_rgb_to_hsv(Vec4f32 rgb) {
        f32 r = rgb.x, g = rgb.y, b = rgb.z;
        f32 M = max(r, max(g, b));
        f32 m = min(r, min(g, b));
        f32 C = M - m;

        f32 h = 0.0f;
        if (C != 0.0f) {
                if (M == r) {
                        h = fmodf((g - b) / C, 6.0f);
                        if (h < 0.0f) {
                                h += 6.0f;
                        }
                } else if (M == g) {
                        h = (b - r) / C + 2.0f;
                } else {
                        h = (r - g) / C + 4.0f;
                }
        }
        h *= M_PI / 3.0f;

        f32 s = 0.0f;
        if (M != 0.0f) {
                s = C / M;
        }

        return v4f32(h, s, M, 1.0f);
}

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

static Arena *ui_widget_arena = { 0 };
static RTexture ui_hsv_color_wheel = { 0 };

static void ui_gen_hsv_color_wheel(void) {
        u32 reso = 512;

        if (!ui_widget_arena) {
                ui_widget_arena = make_arena(reso * reso * sizeof(Vec4u8));
        }
        Vec4u8 *pixels
            = arena_alloc(ui_widget_arena, reso * reso * sizeof(Vec4u8));

        for (u32 y = 0; y < reso; y++) {
                for (u32 x = 0; x < reso; x++) {
                        f32 nx = ((f32)x / reso) * 2 - 1.0f;
                        f32 ny = ((f32)(reso - y) / reso) * 2 - 1.0f;

                        f32 r = sqrt(nx * nx + ny * ny);
                        f32 H = atan2f(ny, nx);

                        if (H < 0) {
                                H += 2.0f * M_PI;
                        }

                        f32 S = clamp(0, r, 1);
                        f32 V = 1;

                        Vec4f32 rgb = ui_hsv_to_rgb(v4f32(H, S, V, 1.0f));
                        // TODO: hr: function to transform a Vec4f32 (rgba) to a
                        // Vec4u8 and vice versa; also used in glyph canvas
                        pixels[y * reso + x]
                            = v4u8(rgb.x * 255, rgb.y * 255, rgb.z * 255, 255);
                }
        }

        r_create_texture((u8 *)pixels, reso, reso, 4, &ui_hsv_color_wheel);
}

static UISignal ui_hsv_color_picker(Vec4f32 *rgba, String8 text) {
        // TODO: hr: add text to strings of child elements
        if (ui_hsv_color_wheel.width == 0) {
                ui_gen_hsv_color_wheel();
        }

        Vec4f32 hsv = ui_rgb_to_hsv(*rgba);
        f32 fontSize = ui_top_text_size();
        f32 height = 4.0f * fontSize;
        f32 pad = fontSize * 0.5f;

        ui_push_border_size(2.0f);
        ui_push_border_color(v4f32(0, 0, 0, 1));

        ui_next_width(uiSizeSumOfChildren(1));
        ui_next_height(uiSizeSumOfChildren(1));
        ui_next_padding(v4f32(pad, pad, pad, pad));
        UIElement *e = ui_build_element_from_string(0, string8_empty());
        UISignal sig = ui_signal_from_element(e);
        e->layoutDirection = UI_Axis2d_X;
        ui_push_parent(e);

        ui_next_width(uiPixelsX(height, 1));
        ui_push_height(uiPixelsY(height, 1));
        ui_next_corner_radius(height);
        ui_next_background_color(v4f32(1, 1, 1, 1));
        UIElement *wheel = ui_build_element_from_string(
            UI_ElementFlag_DrawBackground | UI_ElementFlag_Clickable,
            string8_concat(ui_state.strArena, text, string8_lit(".wheel")));
        ui_element_attach_texture(wheel, &ui_hsv_color_wheel);
        ui_parent(wheel) {
                f32 radius = 0.5f * (height - pad);
                Vec2f32 loc = v2f32(hsv.y * cosf(hsv.x) * radius,
                                    -1.0f * hsv.y * sinf(hsv.x) * radius);
                loc = add_v2f32(loc, v2f32(radius, radius));

                ui_spacer(uiPixelsX(loc.x, 1));
                ui_next_width(uiSizeSumOfChildren(1));
                ui_next_height(uiSizeSumOfChildren(1));
                e = ui_build_element_from_string(0, string8_empty());
                e->layoutDirection = UI_Axis2d_Y;
                ui_push_parent(e);
                ui_spacer(uiPixelsY(loc.y, 1));

                ui_next_width(uiPixelsX(pad, 1));
                ui_next_height(uiPixelsY(pad, 1));
                ui_next_corner_radius(pad);
                ui_next_background_color(v4f32(0, 0, 0, 1));
                e = ui_build_element_from_string(
                    UI_ElementFlag_DrawBackground | UI_ElementFlag_Clickable,
                    string8_concat(ui_state.strArena, text,
                                   string8_lit(".dot")));
                UISignal s = ui_signal_from_element(wheel);
                if (ui_clicked(s) || ui_dragging(s)) {
                        Vec2f32 mousePos = ui_mouse_pos();
                        f32 r = wheel->computedSize.x * 0.5f;
                        Vec2f32 origin
                            = add_v2f32(wheel->screenCoords.min, v2f32(r, r));
                        Vec2f32 pos = sub_v2f32(mousePos, origin);

                        f32 theta = atan2f(pos.y * -1.0f, pos.x);
                        if (theta < 0.0f) {
                                theta += 2.0f * M_PI;
                        }

                        f32 dist = sqrtf(pos.x * pos.x + pos.y * pos.y);

                        hsv.x = theta;
                        hsv.y = clamp_top(dist / r, 1.0f);
                }

                ui_pop_parent();
        }

        ui_spacer(uiPixelsX(pad, 1));

        ui_push_width(uiPixelsX(pad * 1.5f, 1));
        UIElement *bar = ui_build_element_from_string(
            UI_ElementFlag_DrawBackground | UI_ElementFlag_DrawBorder,
            string8_concat(ui_state.strArena, text, string8_lit(".bar")));
        bar->backgroundColors[0] = v4f32(1, 1, 1, 1);
        bar->backgroundColors[1] = v4f32(0, 0, 0, 1);
        bar->backgroundColors[2] = v4f32(1, 1, 1, 1);
        bar->backgroundColors[3] = v4f32(0, 0, 0, 1);
        bar->layoutDirection = UI_Axis2d_Y;
        ui_pop_height();
        ui_parent(bar) {
                ui_spacer(uiPixelsY((1.0f - hsv.z) * (height - pad), 1));

                ui_next_height(uiPixelsX(pad, 1));
                ui_next_background_color(v4f32(0, 0, 0, 1));
                e = ui_build_element_from_string(
                    UI_ElementFlag_DrawBackground | UI_ElementFlag_Clickable,
                    string8_concat(ui_state.strArena, text,
                                   string8_lit(".slide")));
                UISignal s = ui_signal_from_element(bar);
                if (ui_clicked(s) || ui_dragging(s)) {
                        Rng2f32 box = bar->screenCoords;
                        Vec2f32 mousePos = ui_mouse_pos();
                        f32 v = (mousePos.y - box.min.y)
                                / (box.max.y - box.min.y);
                        hsv.z = 1.0f - clamp(0.0f, v, 1.0f);
                }

                ui_pop_width();
        }

        ui_spacer(uiPixelsX(pad, 1));

        ui_next_width(uiSizeSumOfChildren(1));
        ui_next_height(uiSizeSumOfChildren(1));
        e = ui_build_element_from_string(0, string8_empty());
        e->layoutDirection = UI_Axis2d_Y;

        ui_parent(e) {
                ui_next_width(uiPixelsX(fontSize, 1));
                ui_next_height(uiPixelsY(fontSize, 1));
                ui_next_background_color(*rgba);
                e = ui_build_element_from_string(
                    UI_ElementFlag_DrawBackground | UI_ElementFlag_DrawBorder,
                    string8_empty());

                ui_text(string8f(ui_state.strArena, "R: %d",
                                 (s32)(rgba->x * 255.0f)));
                ui_text(string8f(ui_state.strArena, "G: %d",
                                 (s32)(rgba->y * 255.0f)));
                ui_text(string8f(ui_state.strArena, "B: %d",
                                 (s32)(rgba->z * 255.0f)));
        }

        ui_pop_border_color();
        ui_pop_border_size();
        ui_pop_parent();

        *rgba = ui_hsv_to_rgb(hsv);

        return sig;
}
