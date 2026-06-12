#ifndef BEDROCK_CORE_H
#define BEDROCK_CORE_H

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;
typedef uint32_t b32;

#define kb(n) (((u64)(n)) << 10)
#define mb(n) (((u64)(n)) << 20)
#define gb(n) (((u64)(n)) << 30)
#define tb(n) (((u64)(n)) << 40)

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define clamp_top(a, x) min(a, x)
#define clamp_bot(x, b) max(x, b)
#define clamp(a, x, b) (((x) < (a)) ? (a) : ((x) > (b)) ? (b) : (x))

#define mem_zero(m, s) memset((m), 0, (s))

#define array_count(array) sizeof(array) / sizeof(array[0])

#define align_pow2(x, b) (((x) + (b) - 1) & (~((b) - 1)))

#define static_assert(e) typedef char static_assert_##__LINE__[(e) ? 1 : -1]

#endif // BEDROCK_CORE_H
