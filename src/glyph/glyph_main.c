#define STRING8F
#define STRING8_HASH

// clang-format off
#include "bedrock/bedrock_inc.h"
#include "os/os.h"
#include "render/render_inc.h"
#include "font/font.h"
#include "draw/draw.h"
#include "ui/ui_core.h"
#include "ui/ui_inc.h"

#define PERF_IMPLEMENTATION
#include "perf/perf.h"

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
        Arena *permArena;
        Arena *perFrameArena;

        RDynamicTexture canvas;
        b32 pressedPick;

        Vec4f32 currentColor;
        Vec4f32 colorHistory[GLF_COLOR_HISTORY_LEN];
        Vec4f32 colorPicker;

        f32 penSizeFactor;
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

static Vec4u8 glf_color_f32_to_u8(Vec4f32 color) {
        Vec4u8 res
            = v4u8(color.x * 255, color.y * 255, color.z * 255, color.w * 255);
        return res;
}

static Vec4f32 glf_color_u8_to_f32(Vec4u8 color) {
        Vec4f32 res = v4f32((f32)color.x / 255.0f, (f32)color.y / 255.0f,
                            (f32)color.z / 255.0f, (f32)color.w / 255.0f);
        return res;
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

static Vec2s32 glf_canvas_pixel_at_screen_pos(Rng2f32 canvasArea,
                                              Vec2f32 screenPos) {
        if (contains_r2f32(canvasArea, screenPos)) {
                Vec2f32 canvas
                    = add_v2f32(canvasArea.max,
                                v2f32(-canvasArea.min.x, -canvasArea.min.y));
                Vec2f32 mouseRel = add_v2f32(
                    screenPos, v2f32(-canvasArea.min.x, -canvasArea.min.y));
                f32 xf = clamp(0, mouseRel.x / canvas.x, 1);
                f32 yf = clamp(0, mouseRel.y / canvas.y, 1);
                s32 x = xf * glf_state.width;
                s32 y = yf * glf_state.height;
                return v2s32(x, y);
        }
        return v2s32(-1, -1);
}

static void glf_color_history_ui(void) {
        String8 str = string8_lit("dafhiofadnscaifadsgfdfhjflfdjfdafldasf");

        ui_push_width(uiPct(100, 1));
        ui_push_height(uiSizeSumOfChildren(1));
        ui_next_background_color(v4f32(1, 0, 0, 1));
        UIElement *e
            = ui_build_element_from_string(0, string8_lit("top container"));
        ui_push_parent(e);

        ui_push_border_color(v4f32(0, 0, 0, 0));
        ui_push_border_size(4);
        for (u32 r = 0; r < 2; r++) {
                Vec4f32 color = r > 1 ? v4f32(0, 1, 0, 1) : v4f32(0, 0, 1, 1);
                ui_next_background_color(color);
                e = ui_build_element_from_string(
                    0, string8f(ui_state.strArena, "row %d", r));
                e->layoutDirection = UI_Axis2d_X;
                ui_push_parent(e);

                u32 rlen = GLF_COLOR_HISTORY_LEN / 2;
                ui_push_width(uiPct(100.0f / (f32)rlen, 1));
                ui_push_height(uiRatio(1, 1));
                for (u32 c = 0; c < rlen; c++) {
                        u32 i = r * rlen + c;
                        ui_next_background_color(glf_state.colorHistory[i]);
                        String8 strc = string8_skip(str, i);

                        e = ui_build_element_from_string(
                            UI_ElementFlag_DrawBorder
                                | UI_ElementFlag_DrawBackground
                                | UI_ElementFlag_Clickable,
                            strc);

                        UISignal sig = ui_signal_from_element(e);
                        if (ui_clicked(sig)) {
                                glf_pick_current_color_from_history(i);
                        }
                }
                ui_pop_width();
                ui_pop_height();

                ui_pop_parent();
        }
        ui_pop_border_color();
        ui_pop_border_size();

        ui_pop_height();
        ui_pop_width();
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

static void glf_visit_pixel(Vec2s32Node **visited, u32 buckets,
                            Vec2s32Node *pixel) {
        u64 hash = ((u64)pixel->v.x << 32 | (u64)pixel->v.y) % buckets;
        Vec2s32Node *bucket = visited[hash];

        if (!bucket) {
                visited[hash] = pixel;
                pixel->n = 0;
        } else {
                while (bucket->n) {
                        if (equal_v2s32(bucket->n->v, pixel->v)) {
                                break;
                        }
                        bucket = bucket->n;
                }
                bucket->n = pixel;
                pixel->n = 0;
        }
}

// Searches a hashmap visited canvas pixels. Returns 1 if pixel has been visited
// and 0 if not.
static b32 glf_visited_pixel(Vec2s32Node **visited, u32 buckets,
                             Vec2s32 pixel) {
        b32 res = 0;
        u64 hash = ((u64)pixel.x << 32 | (u64)pixel.y) % buckets;
        Vec2s32Node *bucket = visited[hash];

        while (bucket) {
                if (equal_v2s32(bucket->v, pixel)) {
                        res = 1;
                        break;
                }
                bucket = bucket->n;
        }

        return res;
}

typedef b32 (*GLF_FillCanvasCondition)(Vec2s32 pixel, Vec2f32 origin);
static void glf_fill_canvas_on_condition(Vec2f32 pos, Vec4f32 color,
                                         UIElement *canvas,
                                         GLF_FillCanvasCondition cond) {
        if (!canvas) {
                return;
        }
        Vec2s32 pi = glf_canvas_pixel_at_screen_pos(canvas->screenCoords, pos);
        if (pi.x == -1) {
                return;
        }

        Arena *a = glf_state.perFrameArena;
        u64 resetPos = arena_pos(a);
        Vec2s32Node *q = arena_alloc(a, sizeof(*q));
        // hr: guess ideal hashmap size
        u32 buckets = glf_state.width * glf_state.height;
        Vec2s32Node **visited = arena_alloc(a, sizeof(*visited) * buckets);

        q->v = pi;
        Vec2f32 origin = sub_v2f32(pos, canvas->screenCoords.min);
        origin = v2f32(origin.x / canvas->computedSize.x * glf_state.width,
                       origin.y / canvas->computedSize.y * glf_state.height);

        while (q) {
                Vec2s32Node *cur = q;
                q = q->n;

                if (cond(cur->v, origin)) {
                        // hr: fill canvas pixel
                        glf_state.canvas
                            .data[cur->v.y * glf_state.width + cur->v.x]
                            = glf_color_f32_to_u8(color);

                        glf_visit_pixel(visited, buckets, cur);

                        // hr: add unvisted neighbors to the queue
                        Vec2s32 offsets[] = {
                                { { -1, 0 } },
                                { { 0, -1 } },
                                { { 1, 0 } },
                                { { 0, 1 } },
                        };
                        for (u32 i = 0; i < array_count(offsets); i++) {
                                Vec2s32 n = add_v2s32(cur->v, offsets[i]);
                                b32 withinBounds
                                    = n.x >= 0 && n.x < glf_state.width
                                      && n.y >= 0 && n.y < glf_state.height;
                                if (withinBounds
                                    && !glf_visited_pixel(visited, buckets,
                                                          n)) {
                                        Vec2s32Node *node
                                            = arena_alloc(a, sizeof(*node));
                                        node->v = n;
                                        node->n = q;
                                        q = node;
                                }
                        }
                }
        }

        arena_pop_at(a, resetPos);
}

// Returns 1 if pixel is less than current pen size away from origin position on
// canvas, 0 otherwise.
static b32 glf_is_pixel_within_radius(Vec2s32 pixel, Vec2f32 origin) {
        f32 radius = 0.5f
                     + glf_state.penSizeFactor * 0.05f
                           * min(glf_state.width, glf_state.height);

        Vec2f32 c = v2f32((f32)pixel.x + 0.5f, (f32)pixel.y + 0.5f);
        f32 xd = c.x - origin.x;
        f32 yd = c.y - origin.y;
        f32 dist = sqrtf(xd * xd + yd * yd);

        return dist < radius;
}

static Vec4u8 glf_flood_fill_color = { 0 };
static b32 glf_is_pixel_flood_color(Vec2s32 pixel, Vec2f32 origin) {
        Vec4u8 c = glf_state.canvas.data[pixel.y * glf_state.width + pixel.x];
        return equal_v4u8(glf_flood_fill_color, c);
}

static void glf_flood_fill_canvas(Vec2f32 pos, Vec4f32 color,
                                  UIElement *canvas) {
        Vec2s32 i = glf_canvas_pixel_at_screen_pos(canvas->screenCoords, pos);
        Vec4u8 curColor = glf_state.canvas.data[i.y * glf_state.width + i.x];

        Vec4u8 c = glf_color_f32_to_u8(color);
        if (!equal_v4u8(c, curColor)) {
                glf_flood_fill_color = curColor;
                glf_fill_canvas_on_condition(pos, glf_state.currentColor,
                                             canvas, glf_is_pixel_flood_color);
        }
}

int main(int argc, char *argv[]) {

        GLFArgs args = glf_parse_command_line_args(argc, argv);
        if (!args.valid) {
                return 1;
        }

        r_init_backend("glyph", 1000, 800);

        glf_state.permArena = make_arena(gb(1));
        glf_state.perFrameArena = make_arena(mb(48));
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
                r_create_dynamic_texture(glf_state.permArena, glf_state.width,
                                         glf_state.height, &glf_state.canvas);
                glf_copy_image(data, (u8 *)glf_state.canvas.data,
                               glf_state.width, glf_state.height, n, 4);
                stbi_image_free(data);
        } else {
                r_create_dynamic_texture(glf_state.permArena, glf_state.width,
                                         glf_state.height, &glf_state.canvas);
                for (u32 i = 0; i < glf_state.width * glf_state.height; i++) {
                        glf_state.canvas.data[i] = v4u8(255, 255, 255, 255);
                }
        }

        glf_load_control_panel_colors();

        setup_ui_state();
        ui_state.root->layoutDirection = UI_Axis2d_X;

        String8 c = string8_lit("canvas texture");
        String8 cp = string8_lit("control panel");

        Vec4f32 greybg = v4f32(0.22, 0.22, 0.22, 1);
        Vec4f32 darkbg = v4f32(0.16, 0.16, 0.16, 1);
        Vec4f32 greybd = v4f32(0.43, 0.43, 0.43, 1);
        Vec4f32 white = v4f32(1, 1, 1, 1);

        while (!glfwWindowShouldClose(r_state.window)) {

                // TODO: hr: loop management
                b32 res = r_begin_frame();
                if (res) {
                        continue;
                }
                ui_state.root->layoutDirection = UI_Axis2d_Y;
                ui_push_width(uiPct(100, 1));
                ui_push_height(uiPct(100, 1));
                ui_push_background_color(greybg);
                ui_push_border_color(greybd);
                ui_push_text_color(white);

                ui_next_height(uiSizeSumOfChildren(1));
                ui_next_background_color(darkbg);
                f32 padx = uiPixelsX(ui_top_text_size(), 0).value * 0.25f;
                f32 pady = uiPixelsY(ui_top_text_size(), 0).value * 0.25f;
                ui_next_padding(v4f32(padx, pady, padx, pady));
                UIElement *e = ui_build_element_from_string(
                    UI_ElementFlag_DrawBackground, string8_empty());
                e->layoutDirection = UI_Axis2d_X;
                ui_push_parent(e);

                UISignal sig = ui_button(string8_lit("File"));
                if (ui_clicked(sig)) {
                        sig.element->textColor = v4f32(1, 0, 0, 1);
                }
                sig = ui_button(string8_lit("Edit"));
                sig = ui_button(string8_lit("View"));
                ui_spacer(uiPct(100, 0));
                sig = ui_button(string8_lit("Help"));

                ui_pop_parent();

                ui_push_height(uiPct(100, 0));
                e = ui_build_element_from_string(0, string8_empty());
                e->layoutDirection = UI_Axis2d_X;
                ui_push_parent(e);

                ui_next_width(uiPct(20, 1));
                e = ui_build_element_from_string(UI_ElementFlag_DrawBackground,
                                                 string8_empty());

                ui_next_background_color(darkbg);
                ui_next_width(uiPixelsX(ui_pop_text_size() * 0.5, 1));
                e = ui_build_element_from_string(UI_ElementFlag_DrawBackground,
                                                 string8_empty());

                ui_next_width(uiPct(60, 0));
                e = ui_build_element_from_string(UI_ElementFlag_DrawBackground,
                                                 string8_lit("Canvas Area"));
                ui_push_parent(e);

                // TODO: hr: hold canvas aspect ratio, center, and allow
                // zooming

                f32 availAspRatio = e->computedSize.x / e->computedSize.y;
                f32 imageAspRatio
                    = (f32)glf_state.width / (f32)glf_state.height;
                if (availAspRatio < imageAspRatio) {
                        e->layoutDirection = UI_Axis2d_Y;
                        ui_spacer(uiPct(50, 0.5));
                        ui_next_width(uiPct(100, 1));
                        ui_next_height(
                            uiPixels(e->computedSize.x / imageAspRatio, 1));
                        // NOTE: hr: approximating a pixel amount rather
                        // than using uiRatio helps in the case when
                        // there is very little spacing. In the later
                        // the canvas would be bigger than it's parent
                        // because it's parent gets sized down. If the
                        // canvas has strictness 1, then it extends
                        // beyond it's parent's bounds.
                } else {
                        e->layoutDirection = UI_Axis2d_X;
                        ui_spacer(uiPct(50, 0.5));
                        ui_next_width(
                            uiPixels(e->computedSize.y * imageAspRatio, 1));
                        ui_next_height(uiPct(100, 1));
                }

                ui_next_background_color(white);
                UIElement *canvas = ui_build_element_from_string(
                    UI_ElementFlag_DrawBackground, c);
                UISignal canvasSig = ui_signal_from_element(canvas);

                if (ui_dragging(canvasSig)) {
                        glf_fill_canvas_on_condition(
                            ui_mouse_pos(), glf_state.currentColor, canvas,
                            glf_is_pixel_within_radius);
                }

                if (ui_hovering(canvasSig)
                    && glfwGetKey(r_state.window, GLFW_KEY_F) == GLFW_PRESS) {
                        glf_flood_fill_canvas(ui_mouse_pos(),
                                              glf_state.currentColor, canvas);
                }

                RTexture *canvas_tex
                    = r_prep_dynamic_texture(&glf_state.canvas);
                ui_element_attach_texture(canvas, canvas_tex);

                ui_spacer(uiPct(50, 0.5));

                ui_pop_parent();

                ui_next_background_color(darkbg);
                ui_next_width(uiPixelsX(ui_pop_text_size() * 0.5, 1));
                e = ui_build_element_from_string(UI_ElementFlag_DrawBackground,
                                                 string8_empty());

                ui_next_background_color(greybg);
                ui_next_width(uiPct(20, 1));
                e = ui_build_element_from_string(UI_ElementFlag_DrawBackground,
                                                 cp);
                e->layoutDirection = UI_Axis2d_Y;

                ui_push_parent(e);

                String8 colorStr = string8_lit("current color");
                ui_next_width(uiPct(100, 1));
                ui_next_height(uiRatio(1, 1));
                ui_next_background_color(glf_state.currentColor);
                e = ui_build_element_from_string(UI_ElementFlag_DrawBackground,
                                                 colorStr);

                glf_color_history_ui();

                ui_spacer(uiPixelsY(10, 1));
                // NOTE: hr: might be nice to have a fast code path to
                // make a container, size of children, that changes
                // layout direction.
                ui_next_height(uiSizeSumOfChildren(1));
                ui_next_width(uiSizeSumOfChildren(1));
                e = ui_build_element_from_string(0, string8_empty());
                e->layoutDirection = UI_Axis2d_X;
                ui_parent(e) {
                        ui_text(string8_lit("Pen size:"));
                        ui_slider(&glf_state.penSizeFactor, darkbg,
                                  string8_lit("pensize"));
                }
                ui_spacer(uiPixelsY(10, 1));

                ui_hsv_color_picker(&glf_state.colorPicker, string8_lit("hey"));

                ui_spacer(uiPixelsY(10, 1));

                sig = ui_button(string8_lit("Pick color"));
                if (ui_clicked(sig)
                    && !equal_v4f32(glf_state.colorPicker,
                                    glf_state.currentColor)) {
                        glf_set_current_color(glf_state.colorPicker);
                }

                ui_spacer(uiPixelsY(10, 1));

                sig = ui_button(string8_lit("Adjust current color"));
                if (ui_clicked(sig)) {
                        glf_state.colorPicker = glf_state.currentColor;
                }

                ui_spacer(uiPixelsY(20, 1));
                sig = ui_button(string8_lit("Save image"));
                if (ui_clicked(sig)) {
                        glf_save_canvas_to_png();
                }

                if (glfwGetKey(r_state.window, GLFW_KEY_P) == GLFW_PRESS) {
                        glf_state.pressedPick = 1;
                } else if (ui_mouse_over(canvasSig) && glf_state.pressedPick) {
                        Vec2s32 i = glf_canvas_pixel_at_screen_pos(
                            canvas->screenCoords, ui_mouse_pos());
                        if (i.x >= 0) {
                                Vec4u8 color
                                    = glf_state.canvas
                                          .data[i.y * glf_state.width + i.x];
                                glf_set_current_color(
                                    glf_color_u8_to_f32(color));
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
