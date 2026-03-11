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
                b32 res = r_begin_frame();
                if (res) {
                        continue;
                }
                ui_push_text_size(16.0f);

                ui_button(str1);
                if (ui_button(str3).flags & UI_INTERACTIONFLAG_LeftClicked) {
                        glfwSetWindowShouldClose(r_state.window, GLFW_TRUE);
                }
                ui_button(str0);
                ui_slider(&val, str10);

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
