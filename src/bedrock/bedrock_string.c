static String8 string8(u8 *str, u64 length) {
        String8 res = { .data = str, .length = length };
        return res;
}

static String8 string8_allocate(Arena *arena, u64 length) {
        String8 str = { .length = length };
        str.data = arena_alloc(arena, length);
        return str;
}

static u8 string8_at(String8 string, u64 index) {
        if (index < string.length) {
                return string.data[index];
        }
        return 0;
}

static void print_string8(String8 string) {
        for (uint64_t i = 0; i < string.length; i++) {
                printf("%c", string.data[i]);
        }
}

static String8 string8_skip(String8 str, u64 pos) {
        String8 res = {
                .data = str.data + pos,
                .length = str.length - pos,
        };
        return res;
}

static String8 string8_prune(String8 str, u64 pos) {
        String8 res = {
                .data = str.data,
                .length = pos,
        };
        return res;
}

static String8 string8_concat(Arena *arena, String8 str1, String8 str2) {
        String8 res = string8_allocate(arena, str1.length + str2.length);
        memcpy(res.data, str1.data, str1.length);
        memcpy(res.data + str1.length, str2.data, str2.length);
        return res;
}

static bool string8_compare_leading(String8 str1, String8 str2) {
        u64 len = (str1.length > str2.length) ? str2.length : str1.length;
        if (len < 1) {
                return false;
        }

        for (u64 i = 0; i < len; i++) {
                if (str1.data[i] != str2.data[i]) {
                        return false;
                }
        }

        return true;
}

static u64 string8_find_substr(String8 str, String8 substr) {
        String8 tmp = { 0 };
        for (u64 i = 0; i <= str.length - substr.length; i++) {
                tmp = string8_skip(str, i);
                if (string8_compare_leading(tmp, substr)) {
                        return i;
                }
        }
        return UINT64_MAX;
}

#ifndef XXH_IMPLEMENTATION
#define XXH_IMPLEMENTATION
#include "thirdparty/xxHash/xxh3.h"
#endif

static u64 string8_hashkey_from_seed(u64 seed, String8 str) {
        XXH64_hash_t res = XXH3_64bits_withSeed(str.data, str.length, seed);
        return res;
}

static u64 string8_hashkey(String8 str) {
        u64 res = string8_hashkey_from_seed(0x5381, str);
        return res;
}
