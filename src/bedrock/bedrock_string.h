#ifndef BEDROCK_STRING_H
#define BEDROCK_STRING_H

typedef struct string {
  u8 *data;
  u64 length;
} String8;

#define string8_lit(cstr) string8((u8 *)cstr, sizeof(cstr) - 1)

static String8 string8(u8 *str, u64 length); 
static String8 string8_allocate(Arena *arena, u64 length);
static String8 string8fv(Arena *arena, char *fmt, va_list args);
static String8 string8f(Arena *arena, char *fmt, ...);
static u8 string8_at(String8 string, u64 index);
static void print_string8(String8 string);

static String8 string8_skip(String8 str, u64 pos);
static String8 string8_skip_whitespace(String8 str);
static String8 string8_prune(String8 str, u64 pos);

static String8 string8_concat(Arena *arena, String8 str1, String8 str2);

static u64 string8_find_substr(String8 str, String8 substr);

static u64 string8_hashkey_from_seed(u64 seed, String8 str);
static u64 string8_hashkey(String8 str);

#endif // BEDROCK_STRING_H

