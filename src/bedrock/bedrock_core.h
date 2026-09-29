#ifndef BEDROCK_CORE_H
#define BEDROCK_CORE_H

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__clang__)
#define COMPILER_CLANG 1
#else
#define COMPILER_CLANG 0
#endif

#if COMPILER_CLANG && __MACH__
#define readonly __attribute__((section(".ro,data"))) // hr: mach specific?
#else
#define readonly const
#endif

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

#define compile_assert(e) typedef char compile_assert_##__LINE__[(e) ? 1 : -1]

// hr: linked-list helpers
#define CheckNil(nil, p) ((p) == 0 || (p) == nil)
#define SetNil(nil, p) ((p) = nil)

// hr: doubly-linked-lists
#define DLLInsert_NPZ(nil, first, last, prior, node, next, prev)               \
        (CheckNil(nil, first)                                                  \
                 ? ((first) = (last) = (node), SetNil(nil, (node)->next),      \
                    SetNil(nil, (node)->prev))                                 \
         : CheckNil(nil, prior)                                                \
                 ? ((node)->next = (first), (first)->prev = (node),            \
                    (first) = (node), SetNil(nil, (node)->prev))               \
         : ((prior) == (last))                                                 \
                 ? ((last)->next = (node), (node)->prev = (last),              \
                    (last) = (node), SetNil(nil, (node)->next))                \
                 : (((!CheckNil(nil, prior) && CheckNil(nil, (prior)->next))   \
                             ? (0)                                             \
                             : ((prior)->next->prev = (node))),                \
                    ((node)->next = (prior)->next), ((prior)->next = (node)),  \
                    ((node)->prev = (prior))))
#define DLLPushBack_NPZ(nil, first, last, node, next, prev)                    \
        DLLInsert_NPZ(nil, first, last, last, node, next, prev)
#define DLLPushFront_NPZ(nil, first, last, node, next, prev)                   \
        DLLInsert_NPZ(nil, last, first, first, node, prev, next)
#define DLLRemove_NPZ(nil, first, last, node, next, prev)                      \
        (((node) == (first) ? (first) = (node)->next : (0)),                   \
         ((node) == (last) ? (last) = (last)->prev : (0)),                     \
         (CheckNil(nil, (node)->prev) ? (0)                                    \
                                      : ((node)->prev->next = (node)->next)),  \
         (CheckNil(nil, (node)->next) ? (0)                                    \
                                      : ((node)->next->prev = (node)->prev)))

// hr: doubly-linked-list helpers
#define DLLInsert_NP(first, last, prior, node, next, prev)                     \
        DLLInsert_NPZ(0, first, last, prior, node, next, prev)
#define DLLPushBack_NP(first, last, node, next, prev)                          \
        DLLPushBack_NPZ(0, first, last, node, next, prev)
#define DLLPushFront_NP(first, last, node, next, prev)                         \
        DLLPushFront_NPZ(0, first, last, node, next, prev)
#define DLLRemove_NP(first, last, node, next, prev)                            \
        DLLRemove_NPZ(0, first, last, node, next, prev)
#define DLLInsert(first, last, prior, node)                                    \
        DLLInsert_NPZ(0, first, last, prior, node, next, prev)
#define DLLPushBack(first, last, node)                                         \
        DLLPushBack_NPZ(0, first, last, node, next, prev)
#define DLLPushFront(first, last, node)                                        \
        DLLPushFront_NPZ(0, first, last, node, next, prev)
#define DLLRemove(first, last, node)                                           \
        DLLRemove_NPZ(0, first, last, node, next, prev)

// hr: singly-linked, doubly-headed lists (queues)
#define SLLQueuePush_NZ(nil, first, last, node, next)                          \
        (CheckNil(nil, first)                                                  \
                 ? ((first) = (last) = (node), SetNil(nil, (node)->next))      \
                 : ((last)->next = (node), (last) = (node),                    \
                    SetNil(nil, (node)->next)))
#define SLLQueuePushFront_NZ(nil, first, last, node, next)                     \
        (CheckNil(nil, first)                                                  \
                 ? ((first) = (last) = (node), SetNil(nil, (node)->next))      \
                 : ((node)->next = (first), (first) = (node)))
#define SLLQueuePop_NZ(nil, first, last, next)                                 \
        ((first) == (last) ? (SetNil(nil, first), SetNil(nil, last))           \
                           : ((first) = (first)->next))

// hr: singly-linked, doubly-headed list helpers
#define SLLQueuePush_N(first, last, node, next)                                \
        SLLQueuePush_NZ(0, first, last, node, next)
#define SLLQueuePushFront_N(first, last, node, next)                           \
        SLLQueuePushFront_NZ(0, first, last, node, next)
#define SLLQueuePop_N(first, last, next) SLLQueuePop_NZ(0, first, last, next)
#define SLLQueuePush(first, last, node)                                        \
        SLLQueuePush_NZ(0, first, last, node, next)
#define SLLQueuePushFront(first, last, node)                                   \
        SLLQueuePushFront_NZ(0, first, last, node, next)
#define SLLQueuePop(first, last) SLLQueuePop_NZ(0, first, last, next)

// hr: singly-linked, singly-headed list
#define SLLStackPop_N(head, next) ((head) = (head)->next)
#define SLLStackPush_N(head, node, next)                                       \
        ((node)->next = (head), (head) = (node))

// hr: singly-linked, singly-headed list helpers
#define SLLStackPop(head) SLLStackPop_N(head, next)
#define SLLStackPush(head, node) SLLStackPush_N(head, node, next)

#endif // BEDROCK_CORE_H
