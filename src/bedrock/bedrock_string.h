#ifndef BEDROCK_STRING_H
#define BEDROCK_STRING_H

#include "bedrock/bedrock_arena.h"
#include "bedrock/bedrock_core.h"
#include "bedrock/bedrock_freelist.h"

typedef struct string {
        u8 *data;
        u64 length;
} String8;

readonly static String8 string8_nil = { (u8 *)&string8_nil, 0 };

#define string8_lit(cstr) string8((u8 *)cstr, sizeof(cstr) - 1)
#define cstring8_lit(cstr) string8((u8 *)cstr, sizeof(cstr))
#define ccstring8_lit(cstr) { .data = (u8 *)cstr, .length = sizeof(cstr) - 1 }

static String8 string8(u8 *str, u64 length);
static String8 string8_allocate_a(Arena *a, u64 length);
static String8 string8_copy_a(Arena *a, String8 str);
static String8 string8_allocate_f(Freelist *f, u64 length);
static String8 string8_copy_f(Freelist *f, String8 str);
static void string8_destroy_f(Freelist *f, String8 str);
static String8 string8_empty(void);

#ifdef STRING8F
static String8 string8fv(Arena *arena, char *fmt, va_list args);
static String8 string8f(Arena *arena, char *fmt, ...);
#endif

static u8 string8_at(String8 str, u64 index);
static b32 string8_set(String8 str, u64 index, u8 c);
static b32 string8_is_null_term(String8 str);
static void print_string8(String8 str);

static String8 string8_skip(String8 str, u64 pos);
static String8 string8_skip_whitespace(String8 str);
static String8 string8_prune(String8 str, u64 pos);

static String8 string8_concat_a(Arena *a, String8 str1, String8 str2);
static String8 string8_concat_f(Freelist *f, String8 str1, String8 str2);

static u64 string8_find_substr(String8 str, String8 substr);

#ifdef STRING8_HASH
static u64 string8_hashkey_from_seed(u64 seed, String8 str);
static u64 string8_hashkey(String8 str);
#endif

#endif // BEDROCK_STRING_H
