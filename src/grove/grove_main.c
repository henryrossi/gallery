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

static void print_ui_node(UIElement *e, u32 depth) {
        if (e == NULL) {
                return;
        }

        if (e->text.length > 0) {
                for (u32 i = 0; i < depth; i++) {
                        printf("   ");
                }
                print_string8(e->text);
                printf(" size: [%.2f, %.2f], relPos: [%.2f, %.2f], key: %llu\n",
                       e->computedSize.x, e->computedSize.y, e->relPosition.x,
                       e->relPosition.y, e->key);
        }

        print_ui_node(e->firstChild, depth + 1);
        print_ui_node(e->next, depth);
}

static void print_ui_tree() {
        printf("ui state %p\n", ui_state.root);
        print_ui_node(ui_state.root, 1);
}

int main(int argc, char *argv[]) {
        r_init_backend();

        setup_ui_state();
        Arena arena = make_arena(0xF000);

        RTexture t = { 0 };
        r_load_texture("resources/texture.jpg", &t);

        String8 str0 = string8_lit("Button 0");
        String8 str1 = string8_lit("Buttony 1");
        u32 a = 2;
        String8 str10 = string8_concat(
            &arena, str1,
            string8_lit("###This is the hashed part of the string"));
        String8 str3 = string8_lit("El Chalupa");

        while (!glfwWindowShouldClose(r_state.window)) {
                // TODO: hr: loop management
                r_begin_frame();

                UISignal sig = ui_button(str3);

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
