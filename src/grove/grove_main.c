// clang-format off
#include "bedrock/bedrock_inc.h"
#include "os/os_inc.h"
#include "render/render_inc.h"
#include "draw/draw_inc.h"
#include "ui/ui_inc.h"

#include "bedrock/bedrock_inc.c"
#include "os/os_inc.c"
#include "render/render_inc.c"
#include "draw/draw_inc.c"
#include "ui/ui_inc.c"

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
        setup_ui_state();
        Arena arena = make_arena(0xF000);

        r_init_backend();

       RTexture t = { 0 };
        r_load_texture("resources/texture.jpg", &t);

        String8 str0 = string8_lit("Button 0");
        String8 str1 = string8_lit("Button 1");
        u32 a = 2;
        String8 str10 = string8_concat(
            &arena, str1,
            string8_lit("###This is the hashed part of the string"));
        String8 str3 = string8_lit("Button 3");

        while (!glfwWindowShouldClose(r_state.window)) {
                // TODO: hr: loop management
                u64 frame = r_get_frame_count();
                ui_cache_prune(frame);
                r_begin_frame();

                dr_rect(r2f32p(1000, 300, 1250, 500), v4f32(1, 1, 1, 1), 0, 0);
                dr_text(0, 40.0, str0, r2f32p(1000, 300, 1240, 500), v4f32(0, 0, 0, 1));
                dr_img(r2f32p(1000, 500, 1500, 1000), v4f32(1, 1, 1, 1), &t,
                       r2f32p(0, 0, 1, 1), 50, 0);
                ui_push_text_color(v4f32(0.5f, 1, 1, 1));
                ui_pop_text_color();
                ui_push_text_color(v4f32(0.2f, 1, 1, 1));

                ui_push_background_color(v4f32(1, 1, 0, 1));

                ui_push_width(
                    uiSemanticSize(UI_SIZEKIND_PercentOfParent, 25.0f, 0));
                ui_push_height(
                    uiSemanticSize(UI_SIZEKIND_PercentOfParent, 25.0f, 0));

                ui_push_height(uiSizeSumOfChildren());
                UISignal sig0 = ui_button(str0);
                ui_pop_height();
                ui_push_parent(sig0.element);

                ui_push_background_color(v4f32(1, 0.5, 0.5, 1));
                ui_push_width(uiSemanticSize(UI_SIZEKIND_Pixels, 100.0f, 0));
                ui_push_height(uiSemanticSize(UI_SIZEKIND_Pixels, 100.0f, 0));
                UISignal sig = ui_button(str1);
                ui_pop_width();
                ui_pop_height();
                ui_push_parent(sig.element);

                ui_push_background_color(v4f32(0, 0, 1, 1));

                ui_buttonf("Button %d", a);
                ui_pop_parent();

                ui_push_background_color(v4f32(1, 0, 1, 1));

                ui_push_width(uiSemanticSize(UI_SIZEKIND_Pixels, 100.0f, 0));
                ui_push_height(uiSemanticSize(UI_SIZEKIND_Pixels, 100.0f, 0));
                ui_button(str10);
                ui_pop_width();
                ui_pop_height();
                ui_pop_parent();

                ui_button(str3);

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
