// clang-format off
#include "bedrock/bedrock_inc.h"
#include "os/os_inc.h"
#include "render/render_inc.h"
#include "font/font.h"
#include "draw/draw.h"
#include "ui/ui_inc.h"

#include "bedrock/bedrock_inc.c"
#include "os/os_inc.c"
#include "render/render_inc.c"
#include "font/font.c"
#include "draw/draw.c"
#include "ui/ui_inc.c"
// clang-format on

int main(int argc, char *argv[]) {
        r_init_backend();

        setup_ui_state();
        Arena arena = make_arena(0xF000);

        RTexture t = { 0 };
        r_load_texture("resources/texture.jpg", &t);

        String8 str0 = string8_lit("Button 0");
        String8 str1 = string8_lit("Buttony 1");
        String8 str10 = string8_concat(
            &arena, str1,
            string8_lit("###This is the hashed part of the string"));
        String8 str3 = string8_lit("El Chalupa");

        while (!glfwWindowShouldClose(r_state.window)) {
                // TODO: hr: loop management
                r_begin_frame();

                ui_button(str1);
                if (ui_button(str3).flags & UI_INTERACTIONFLAG_LeftClicked) {
                        glfwSetWindowShouldClose(r_state.window, GLFW_TRUE);
                }
                ui_button(str0);

                ui_element_autolayout();
                ui_draw_elements();

                // TODO: hr: more loop management
                r_end_frame();
        }

        r_destroy_texture(&t);
        // r_destroy_texture(&dr_font);
        r_destroy_backend();

        return 0;
}
