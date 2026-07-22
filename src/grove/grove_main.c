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
        Arena *arena = make_arena(kb(6));

        RTexture t = { 0 };
        r_load_texture("resources/texture.jpg", &t);

        String8 str0 = string8_lit("Button 0");
        String8 str1 = string8_lit("Buttony 1");
        String8 str10 = string8_concat(
            arena, str1,
            string8_lit("###This is the hashed part of the string"));
        String8 str3 = string8_lit("El Chalupa");

        f32 val = 0;
        while (!glfwWindowShouldClose(r_state.window)) {
                // TODO: hr: loop management
                b32 res = ui_begin_frame();
                if (res) {
                        continue;
                }
                ui_push_text_size(16.0f);

                ui_button(str1);
                if (ui_clicked(ui_button(str3))) {
                        glfwSetWindowShouldClose(r_state.window, GLFW_TRUE);
                }
                ui_button(str0);
                ui_slider(&val, v4f32(1, 1, 1, 1), str10);

                Vec4f32 red = v4f32(1, 0, 0, 1);
                Vec4f32 yellow = v4f32(1, 1, 0, 1);
                Vec4f32 blue = v4f32(0, 0, 1, 1);

                ui_next_background_color(red);
                ui_next_width(uiPixelsX(500, 1));
                ui_next_height(uiPixelsY(500, 1));
                UIElement *e = ui_build_element_from_string(
                    UI_ElementFlag_DrawBackground | UI_ElementFlag_Clip,
                    string8_lit("box"));
                ui_push_parent(e);

                ui_next_background_color(yellow);
                ui_next_width(uiPixelsX(1000, 1));
                ui_next_height(uiPixelsY(200, 1));
                e = ui_build_element_from_string(UI_ElementFlag_DrawBackground,
                                                 string8_empty());
                ui_element_attach_texture(e, &t);
                ui_push_parent(e);

                ui_next_background_color(blue);
                ui_next_width(uiPixelsX(1500, 1));
                ui_next_height(uiPixelsY(100, 1));
                e = ui_build_element_from_string(UI_ElementFlag_DrawBackground,
                                                 string8_empty());
                ui_element_attach_texture(e, &t);

                ui_element_autolayout();
                ui_draw_elements();

                // TODO: hr: more loop management
                r_end_frame();
        }

        r_destroy_texture(&t);
        f_destroy_font(ui_state.defaultFont);
        r_destroy_backend();

        return 0;
}
