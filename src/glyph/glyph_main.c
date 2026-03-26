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

typedef struct {
        char *filename;
        u8 *pixels;
        u32 width;
        u32 height;
        u32 channels;
} GLFImageView;

#define GLF_COLOR_HISTORY_LEN 16
typedef struct {
        const char *filename;
        u32 width;
        u32 height;
        Arena *arena;

        RDynamicTexture canvas;
        b32 pressedPick;

        Vec4f32 currentColor;
        Vec4f32 colorHistory[GLF_COLOR_HISTORY_LEN];
        Vec4f32 colorPicker;
} GLFState;

GLFState glf_state = { 0 };

#include "glyph/cli.c"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "thirdparty/stb/stb_image.h"
#endif
#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "thirdparty/stb/stb_image_write.h"
#endif

static int glf_save_canvas_to_png(void) {
        return stbi_write_png(glf_state.filename, glf_state.width,
                              glf_state.height, 4, glf_state.canvas.data,
                              glf_state.width * 4);
}

static u8 *glf_read_canvas_input_file(const char *filename, u32 *width,
                                      u32 *height, u32 *n) {
        u8 *data
            = stbi_load(filename, (int *)width, (int *)height, (int *)n, 0);
        if (!data) {
                fprintf(stderr, "Failed to read input file: %s\n", filename);
                return 0;
        }

        return data;
}

static void glf_set_current_color(Vec4f32 color) {
        for (u32 i = GLF_COLOR_HISTORY_LEN - 1; i > 0; i--) {
                glf_state.colorHistory[i] = glf_state.colorHistory[i - 1];
        }
        glf_state.colorHistory[0] = glf_state.currentColor;
        glf_state.currentColor = color;
}

static void glf_pick_current_color_from_history(u32 picked) {
        Vec4f32 color = glf_state.colorHistory[picked];
        for (u32 i = picked; i > 0; i--) {
                glf_state.colorHistory[i] = glf_state.colorHistory[i - 1];
        }
        glf_state.colorHistory[0] = glf_state.currentColor;
        glf_state.currentColor = color;
}

static void glf_load_control_panel_colors(void) {
        const static char *csFilename = "color.save";
        FILE *fp = fopen(csFilename, "rb");
        if (fp) {
                // fread(colors, sizeof(colors), 1, fp);
        }
        for (u32 i = 0; i < GLF_COLOR_HISTORY_LEN; i++) {
                glf_state.colorHistory[i] = v4f32(1, 1, 1, 1);
        }

        glf_state.currentColor = v4f32(1, 1, 1, 1);
        glf_state.colorPicker = v4f32(1, 1, 1, 1);
}

static s32 glf_canvas_pixel_at_pos(Rng2f32 extent) {
        Vec2f32 mouse = ui_mouse_pos();
        if (contains_r2f32(extent, mouse)) {
                Vec2f32 canvas = add_v2f32(extent.max,
                                           v2f32(-extent.min.x, -extent.min.y));
                Vec2f32 mouseRel
                    = add_v2f32(mouse, v2f32(-extent.min.x, -extent.min.y));
                f32 xf = clamp(0, mouseRel.x / canvas.x, 1);
                f32 yf = clamp(0, mouseRel.y / canvas.y, 1);
                u32 x = xf * glf_state.width;
                u32 y = yf * glf_state.height;
                return y * glf_state.width + x;
        }
        return -1;
}

static void glf_color_history_ui(void) {
        String8 str = string8_lit("dafhiofadnscaifadsgfdfhjflfdjfdafldasf");
        ui_push_width(uiPct(100, 0));
        ui_push_height(uiSizeSumOfChildren(0));
        UIElement *e = ui_build_element_from_string(0, string8_empty());
        ui_push_parent(e);

        ui_push_border_color(v4f32(0, 0, 0, 0));
        ui_push_border_size(4);
        for (u32 r = 0; r < 2; r++) {
                e = ui_build_element_from_string(0, string8_empty());
                e->layoutDirection = UI_AXIS2D_X;
                ui_push_parent(e);

                u32 rlen = GLF_COLOR_HISTORY_LEN / 2;
                ui_push_width(uiPct(100.0f / (f32)rlen, 0));
                ui_push_height(uiRatio(1, 0));
                for (u32 c = 0; c < rlen; c++) {
                        u32 i = r * rlen + c;
                        ui_push_background_color(glf_state.colorHistory[i]);
                        String8 strc = string8_skip(str, i);
                        e = ui_build_element_from_string(
                            UI_ELEMENTFLAG_DrawBorder
                                | UI_ELEMENTFLAG_DrawBackground
                                | UI_ELEMENTFLAG_Clickable,
                            strc);
                        UISignal sig = ui_signal_from_element(e);
                        if (sig.flags & UI_INTERACTIONFLAG_LeftClicked) {
                                glf_pick_current_color_from_history(i);
                        }
                        ui_pop_background_color();
                }
                ui_pop_width();
                ui_pop_height();

                ui_pop_parent();
        }
        ui_pop_border_color();
        ui_pop_border_size();

        ui_pop_height();
        ui_pop_parent();
}

static void glf_copy_image(u8 *src, u8 *dst, u32 width, u32 height, u32 srcN,
                           u32 dstN) {
        if (srcN < 3 || dstN < 3) {
                os_abort(1);
        }

        for (u32 y = 0; y < height; y++) {
                for (u32 x = 0; x < width; x++) {
                        u32 si = (y * width + x) * srcN;
                        u32 di = (y * width + x) * dstN;
                        dst[di] = src[si];
                        dst[di + 1] = src[si + 1];
                        dst[di + 2] = src[si + 2];
                        if (srcN == 3 && dstN == 4) {
                                dst[di + 3] = 255;
                        } else if (srcN == 4 && dstN == 4) {
                                dst[di + 3] = src[si + 3];
                        }
                }
        }
}

int main(int argc, char *argv[]) {
        GLFArgs args = glf_parse_command_line_args(argc, argv);
        if (!args.valid) {
                return 1;
        }

        r_init_backend("glyph", 1000, 800);

        glf_state.arena = make_arena(gb(1));
        glf_state.filename = args.filename;
        glf_state.width = args.width;
        glf_state.height = args.height;

        if (glf_state.width == 0) {
                u32 n = 0;
                u8 *data = glf_read_canvas_input_file(glf_state.filename,
                                                      &glf_state.width,
                                                      &glf_state.height, &n);
                if (!data) {
                        return 1;
                }
                r_create_dynamic_texture(glf_state.arena, glf_state.width,
                                         glf_state.height, &glf_state.canvas);
                glf_copy_image(data, (u8 *)glf_state.canvas.data,
                               glf_state.width, glf_state.height, n, 4);
                stbi_image_free(data);
        } else {
                r_create_dynamic_texture(glf_state.arena, glf_state.width,
                                         glf_state.height, &glf_state.canvas);
                for (u32 i = 0; i < glf_state.width * glf_state.height; i++) {
                        glf_state.canvas.data[i] = v4u8(255, 255, 255, 255);
                }
        }

        glf_load_control_panel_colors();

        setup_ui_state();
        ui_state.root->layoutDirection = UI_AXIS2D_X;

        String8 c = string8_lit("canvas texture");
        String8 cp = string8_lit("control panel");

        while (!glfwWindowShouldClose(r_state.window)) {
                // TODO: hr: loop management
                b32 res = r_begin_frame();
                if (res) {
                        continue;
                }
                ui_state.root->layoutDirection = UI_AXIS2D_X;

                ui_push_height(uiPct(100, 0));
                ui_push_width(uiPct(75, 0));

                UIElement *canvas = ui_build_element_from_string(
                    UI_ELEMENTFLAG_DrawBackground, c);
                UISignal canvasSig = ui_signal_from_element(canvas);
                if (canvasSig.flags & UI_INTERACTIONFLAG_LeftDragging) {
                        s32 i = glf_canvas_pixel_at_pos(canvas->screenCoords);
                        if (i > 0) {
                                Vec4f32 c = glf_state.currentColor;
                                glf_state.canvas.data[i] = v4u8(
                                    c.x * 255, c.y * 255, c.z * 255, c.w * 255);
                        }
                }
                RTexture *canvas_tex
                    = r_prep_dynamic_texture(&glf_state.canvas);
                ui_element_attach_texture(canvas, canvas_tex);

                ui_pop_width();

                ui_push_background_color(v4f32(0.4, 0.4, 0.45, 1));
                ui_push_width(uiPct(25, 0));
                UIElement *e = ui_build_element_from_string(
                    UI_ELEMENTFLAG_DrawBackground, cp);
                e->layoutDirection = UI_AXIS2D_Y;

                ui_push_parent(e);

                String8 colorStr = string8_lit("current color");
                ui_push_width(uiPct(100, 0));
                ui_push_height(uiRatio(1, 0));
                ui_push_background_color(glf_state.currentColor);
                e = ui_build_element_from_string(UI_ELEMENTFLAG_DrawBackground,
                                                 colorStr);
                ui_pop_background_color();

                glf_color_history_ui();

                ui_spacer(uiPixelsY(20, 0));
                ui_push_background_color(glf_state.colorPicker);
                ui_push_width(uiPixelsX(100, 0));
                ui_push_height(uiPixelsY(100, 0));
                e = ui_build_element_from_string(UI_ELEMENTFLAG_DrawBackground,
                                                 string8_lit("preview"));
                ui_pop_background_color();
                ui_pop_width();
                ui_pop_height();

                ui_push_background_color(v4f32(0.7, 0.7, 0.75, 1));
                ui_push_text_size(16.0f);
                ui_spacer(uiPixelsY(10, 0));
                ui_slider(&glf_state.colorPicker.x, string8_lit("preview red"));
                ui_spacer(uiPixelsY(10, 0));
                ui_slider(&glf_state.colorPicker.y,
                          string8_lit("preview green"));
                ui_spacer(uiPixelsY(10, 0));
                ui_slider(&glf_state.colorPicker.z,
                          string8_lit("preview blue"));
                ui_spacer(uiPixelsY(10, 0));
                ui_slider(&glf_state.colorPicker.w,
                          string8_lit("preview alpha"));

                ui_spacer(uiPixelsY(10, 0));
                UISignal sig = ui_button(string8_lit("Pick color"));
                if (sig.flags & UI_INTERACTIONFLAG_LeftClicked) {
                        glf_set_current_color(glf_state.colorPicker);
                }

                ui_spacer(uiPixelsY(20, 0));
                sig = ui_button(string8_lit("Save image"));
                if (sig.flags & UI_INTERACTIONFLAG_LeftClicked) {
                        glf_save_canvas_to_png();
                }

                ui_pop_text_size();
                ui_pop_background_color();

                ui_pop_width();
                ui_pop_height();

                ui_pop_parent();

                ui_pop_width();
                ui_pop_height();
                ui_pop_background_color();

                if (glfwGetKey(r_state.window, GLFW_KEY_P) == GLFW_PRESS) {
                        glf_state.pressedPick = 1;
                } else if (canvasSig.flags & UI_INTERACTIONFLAG_MouseOver
                           && glf_state.pressedPick) {
                        s32 i = glf_canvas_pixel_at_pos(canvas->screenCoords);
                        if (i > 0) {
                                Vec4u8 pixel = glf_state.canvas.data[i];
                                glf_set_current_color(
                                    v4f32((f32)pixel.x / 255.0f,
                                          (f32)pixel.y / 255.0f,
                                          (f32)pixel.z / 255.0f,
                                          (f32)pixel.w / 255.0f));
                        }
                        glf_state.pressedPick = 0;
                }

                ui_element_autolayout();
                ui_draw_elements();

                // TODO: hr: more loop management
                r_end_frame();
        }

        f_destroy_font(ui_state.defaultFont);
        r_destroy_dynamic_texture(&glf_state.canvas);
        r_destroy_backend();

        return 0;
}
