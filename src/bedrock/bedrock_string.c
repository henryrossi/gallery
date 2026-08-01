#include "bedrock/bedrock_string.h"

static String8 string8(u8 *str, u64 length) {
        String8 res = { .data = str, .length = length };
        return res;
}

static String8 string8_allocate_a(Arena *a, u64 length) {
        String8 res = { .length = length };
        res.data = arena_alloc(a, length);
        return res;
}

static String8 string8_copy_a(Arena *a, String8 str) {
        String8 res = string8_allocate_a(a, str.length);
        memcpy(res.data, str.data, str.length);
        return res;
}

static String8 string8_allocate_f(Freelist *f, u64 length) {
        String8 res = { .length = length };
        res.data = freelist_alloc(f, length);
        return res;
}

static String8 string8_copy_f(Freelist *f, String8 str) {
        String8 res = string8_allocate_f(f, str.length);
        memcpy(res.data, str.data, str.length);
        return res;
}

static void string8_destroy_f(Freelist *f, String8 str) {
        freelist_free(f, str.data);
}

static String8 string8_empty(void) {
        String8 res = { 0 };
        return res;
}

#ifdef STRING8F
#ifndef STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_STATIC
#include "thirdparty/stb/stb_sprintf.h"
#endif
static String8 string8fv(Arena *arena, char *fmt, va_list args) {
        va_list args2;
        va_copy(args2, args);
        u32 bytesNeeded = stbsp_vsnprintf(0, 0, fmt, args) + 1;
        String8 str = string8_allocate_a(arena, bytesNeeded);
        str.length = stbsp_vsnprintf((char *)str.data, bytesNeeded, fmt, args);
        va_end(args2);
        return str;
}

static String8 string8f(Arena *arena, char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        String8 res = string8fv(arena, fmt, args);
        va_end(args);
        return res;
}
#endif

static u8 string8_at(String8 str, u64 index) {
        if (index < str.length) {
                return str.data[index];
        }
        return 0;
}

static b32 string8_set(String8 str, u64 index, u8 c) {
        if (str.length <= index) {
                return 0;
        }
        str.data[index] = c;
        return 1;
}

static b32 string8_is_null_term(String8 str) {
        return string8_at(str, str.length - 1) == '\0';
}

static void print_string8(String8 string) {
        for (uint64_t i = 0; i < string.length; i++) {
                printf("%c", string.data[i]);
        }
}

static String8 string8_skip(String8 str, u64 pos) {
        if (pos > str.length) {
                pos = str.length;
        }
        str.data += pos;
        str.length -= pos;
        return str;
}

static String8 string8_skip_whitespace(String8 str) {
        u32 count = 0;
        u32 done = 0;
        while (!done) {
                u8 c = string8_at(str, count);
                if (c == '\t' || c == ' ' || c == '\n') {
                        count++;
                } else {
                        done = 1;
                }
        }
        str = string8_skip(str, count);
        return str;
}

static String8 string8_prune(String8 str, u64 pos) {
        if (pos < str.length) {
                str.length = pos;
        }
        return str;
}

static String8 string8_concat_a(Arena *a, String8 str1, String8 str2) {
        String8 res = string8_allocate_a(a, str1.length + str2.length);
        memcpy(res.data, str1.data, str1.length);
        memcpy(res.data + str1.length, str2.data, str2.length);
        return res;
}

static String8 string8_concat_f(Freelist *f, String8 str1, String8 str2) {
        String8 res = string8_allocate_f(f, str1.length + str2.length);
        memcpy(res.data, str1.data, str1.length);
        memcpy(res.data + str1.length, str2.data, str2.length);
        return res;
}

static b32 string8_compare_leading(String8 str1, String8 str2) {
        u64 len = (str1.length > str2.length) ? str2.length : str1.length;
        if (len < 1) {
                return 0;
        }

        for (u64 i = 0; i < len; i++) {
                if (str1.data[i] != str2.data[i]) {
                        return 0;
                }
        }
        return 1;
}

static u64 string8_find_substr(String8 str, String8 substr) {
        String8 tmp = { 0 };
        if (str.length == 0) {
                return UINT64_MAX;
        }
        for (u64 i = 0; i <= str.length - substr.length; i++) {
                tmp = string8_skip(str, i);
                if (string8_compare_leading(tmp, substr)) {
                        return i;
                }
        }
        return UINT64_MAX;
}

#ifdef STRING8_HASH
#ifndef XXH_IMPLEMENTATION
#define XXH_IMPLEMENTATION
#include "thirdparty/xxHash/xxh3.h"
#endif

static u64 string8_hashkey_from_seed(u64 seed, String8 str) {
        if (str.length == 0) {
                return 0;
        }
        XXH64_hash_t res = XXH3_64bits_withSeed(str.data, str.length, seed);
        return res;
}

static u64 string8_hashkey(String8 str) {
        u64 res = string8_hashkey_from_seed(0x5381, str);
        return res;
}
#endif
