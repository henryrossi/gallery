#ifndef DRAW_H
#define DRAW_H

// hr: tmp 
typedef struct {
	String8 name;
} FFont;

// Functions
// dr_rect
// dr_img
// dr_text
//
// these functions call functions from the render layer

static void dr_rect(Rng2f32 rng, Vec4f32 color, f32 cornerRadius, f32 edgeSoftness);

static void dr_img(Rng2f32 rng, Vec4f32 color, String8 filename, Rng2f32 srcRng);

static void dr_text(FFont *f, f32 size, String8 text, Vec2f32 pos0, Vec4f32 color);

#endif // DRAW_H
