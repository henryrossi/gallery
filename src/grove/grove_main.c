#define STRING8F
#define STRING8_HASH

// clang-format off
#include "bedrock/bedrock_inc.h"
#include "os/os.h"
#include "render/render_inc.h"
#include "font/font.h"
#include "draw/draw.h"
#include "ui/ui_inc.h"

#include "bedrock/bedrock_inc.c"
#include "os/os.c"
#include "render/render_inc.c"
#include "font/font.c"
#include "draw/draw.c"
#include "ui/ui_inc.c"
// clang-format on

int main(int argc, char *argv[]) {
        r_init_backend("grove", 1000, 800);

        setup_ui_state();
        // Arena *arena = make_arena(kb(6));

        RTexture t = { 0 };
        r_load_texture("resources/texture.jpg", &t);

        Vec4f32 red = v4f32(1, 0, 0, 1);
        Vec4f32 yellow = v4f32(1, 1, 0, 1);
        Vec4f32 blue = v4f32(0, 0, 1, 1);
        Vec4f32 green = v4f32(0, 1, 0, 1);

        b32 clip = 0;
        b32 vertical = 0;
        f32 redSize = 100.0f;
        f32 blueSize = 100.0f;
        f32 greenSize = 100.0f;

        while (!glfwWindowShouldClose(r_state.window)) {
                // TODO: hr: loop management
                b32 res = ui_begin_frame();
                if (res) {
                        continue;
                }
                ui_push_text_size(16.0f);

                ui_push_width(uiPct(100, 1));
                ui_push_height(uiSizeSumOfChildren(1));
                UIElement *e = ui_build_element_from_string(0, string8_empty());
                ui_push_parent(e);

                // TODO: hr: mass ui style push with macros? Single ui element
                // mass set (mass set next)?

                e = ui_build_element_from_string(0, string8_empty());
                e->layoutDirection = UI_Axis2d_X;
                ui_push_parent(e);
                if (ui_clicked(ui_button(string8_lit("swap clip")))) {
                        clip = !clip;
                }
                if (ui_clicked(ui_button(string8_lit("swap direction")))) {
                        vertical = !vertical;
                }
                ui_pop_parent();

                // ui_direction_layout style stack?
                e = ui_build_element_from_string(0, string8_empty());
                e->layoutDirection = UI_Axis2d_X;
                ui_push_parent(e);
                ui_text(string8_lit("Red box: "));
                if (ui_clicked(ui_button(string8_lit("+##redinc")))) {
                        redSize = clamp(0, redSize + 50, 1000);
                }
                if (ui_clicked(ui_button(string8_lit("-##reddec")))) {
                        redSize = clamp(0, redSize - 50, 1000);
                }
                ui_pop_parent();

                e = ui_build_element_from_string(0, string8_empty());
                e->layoutDirection = UI_Axis2d_X;
                ui_push_parent(e);
                ui_text(string8_lit("Blue box: "));
                if (ui_clicked(ui_button(string8_lit("+##blueinc")))) {
                        blueSize = clamp(0, blueSize + 50, 500);
                }
                if (ui_clicked(ui_button(string8_lit("-###bluedec")))) {
                        blueSize = clamp(0, blueSize - 50, 500);
                }
                ui_pop_parent();

                e = ui_build_element_from_string(0, string8_empty());
                e->layoutDirection = UI_Axis2d_X;
                ui_push_parent(e);
                ui_text(string8_lit("Green box: "));
                if (ui_clicked(ui_button(string8_lit("+##greeninc")))) {
                        greenSize = clamp(0, greenSize + 50, 500);
                }
                if (ui_clicked(ui_button(string8_lit("-###greendec")))) {
                        greenSize = clamp(0, greenSize - 50, 500);
                }
                ui_pop_parent();

                ui_pop_width();
                ui_pop_height();
                ui_pop_parent();

                e = ui_build_element_from_string(0, string8_empty());
                e->layoutDirection = UI_Axis2d_X;
                ui_push_parent(e);
                ui_spacer(uiPixelsX(200, 1));

                ui_next_width(uiPixelsX(400, 1));
                ui_next_height(uiPixelsX(400, 1));
                UI_ElementFlags flags = UI_ElementFlag_DrawBackground;
                flags |= (UI_ElementFlag_overflowVisibleX * clip);
                ui_next_background_color(yellow);
                e = ui_build_element_from_string(flags,
                                                 string8_lit("yellow parent"));
                e->layoutDirection = vertical ? UI_Axis2d_Y : UI_Axis2d_X;
                ui_parent(e) {
                        flags = UI_ElementFlag_DrawBackground;
                        ui_next_width(uiPixelsX(redSize, 1));
                        ui_next_height(uiPixelsX(redSize, 1));
                        ui_next_background_color(red);
                        e = ui_build_element_from_string(
                                flags, string8_lit("red child"));
                        ui_element_attach_texture(e, &t);
                        ui_next_width(uiPixelsX(blueSize, 1));
                        ui_next_height(uiPixelsX(blueSize, 1));
                        ui_next_background_color(blue);
                        e = ui_build_element_from_string(
                                flags, string8_lit("blue child"));
                        ui_element_attach_texture(e, &t);

                        ui_parent(e) {
                                ui_next_width(uiPixelsX(greenSize, 1));
                                ui_next_height(uiPixelsX(greenSize, 1));
                                ui_next_background_color(green);
                                UIElement *eg = ui_build_element_from_string(
                                        flags, string8_lit("green subchild"));
                                ui_element_attach_texture(eg, &t);
                        }
                }

                ui_pop_parent();

                ui_end_build();
                ui_draw_elements();

                // TODO: hr: more loop management
                r_end_frame();
        }

        r_destroy_texture(&t);
        f_destroy_font(ui_state.defaultFont);
        r_destroy_backend();

        return 0;
}
