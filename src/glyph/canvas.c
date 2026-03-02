

// Creates a texture image. Returns 1 on success, 0 on failure.
static int gl_create_canvas(gl_state *state) {
        Canvas *canvas = &state->canvas;
        if (canvas->fileCreated) {
                canvas->size = canvas->width * canvas->height * 4;
                canvas->data = malloc(canvas->size);
                for (uint32_t i = 0; i < canvas->size; i++) {
                        canvas->data[i] = 255;
                }
        } else {
                if (!gl_read_canvas_input_file(&state->canvas)) {
                        return 0;
                }
        }

        return 1;
}

static void gl_destroy_canvas(gl_state *state) {
        // r_destroy_dynamic_texture
}
