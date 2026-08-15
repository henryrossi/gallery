#include "bedrock/bedrock_freelist.h"
#include "bedrock/bedrock_logs.h"
#include "os/os.h"

// NOTE: hr: The freelist allocator manages it's free chunks using a red-black
// tree. For more info see:
// --
// "Introduction to Algorithms" Corman, Leiserson, Rivest, Stein - 2022. Chapter
// 13 Red-Black Trees
// --
// "Memory Allocation Strategies" Ginger Bill. Part 5 - Free List Allocators
// https://www.gingerbill.org/article/2021/11/30/memory-allocation-strategies-005/

#define FREELIST_HEADER_SIZE 32

typedef enum {
        RBTree_Black = 0,
        RBTree_Red = 1,
} RBTree_Color;

typedef struct {
        u64 sizeAndFlags;
} FreelistInfo;

struct FreelistNode {
        FreelistInfo info;
        FreelistNode *left;
        FreelistNode *right;
        FreelistNode *p;
};

typedef struct {
        FreelistInfo info;
        u64 padding;
} FreelistHeader;

compile_assert(sizeof(FreelistNode) <= FREELIST_HEADER_SIZE);
compile_assert(sizeof(FreelistHeader) <= FREELIST_HEADER_SIZE);

#define FLRBT_COLOR_BIT (1 << 0)
#define _flrbt_is_red(node) (!!(node->info.sizeAndFlags & FLRBT_COLOR_BIT))
#define _flrbt_set_red(node) node->info.sizeAndFlags |= FLRBT_COLOR_BIT
#define _flrbt_set_black(node) node->info.sizeAndFlags &= ~FLRBT_COLOR_BIT
#define _flrbt_assign_color(x, y)                                              \
        if (_flrbt_is_red(y)) {                                                \
                _flrbt_set_red(x);                                             \
        } else {                                                               \
                _flrbt_set_black(x);                                           \
        }
#define FLRBT_FREE_BIT (1 << 1)
#define _flrbt_is_free(nh) (!!(nh->info.sizeAndFlags & FLRBT_FREE_BIT))
#define _flrbt_set_free(node) node->info.sizeAndFlags |= FLRBT_FREE_BIT
#define _flrbt_set_allocated(header)                                           \
        header->info.sizeAndFlags &= ~FLRBT_FREE_BIT
#define _flrbt_size(nh) (nh->info.sizeAndFlags & ~0x3)

// NOTE: hr: this struct is NOT read only. Several times the red-black tree
// algorithms use the parent field as a temporary variable. All algoritms that
// read from the parent field guarantee a valid write to the field sometime
// beforehand.
static FreelistNode freelist_node_nil = {
        { 0 },
        &freelist_node_nil,
        &freelist_node_nil,
        &freelist_node_nil,
};

static b32 _flrbt_node_is_nil(FreelistNode *x) {
        return x == &freelist_node_nil;
}

static b32 _flrbt_validate(FreelistNode *x, u64 blackBalance) {
        if (_flrbt_node_is_nil(x)) {
                return blackBalance == 0;
        }

        if (!_flrbt_is_red(x)) {
                blackBalance--;
        } else {
                if (_flrbt_is_red(x->left) || _flrbt_is_red(x->right)) {
                        return 0;
                }
        }

        if (!_flrbt_node_is_nil(x->left)
            && _flrbt_size(x) < _flrbt_size(x->left)) {
                return 0;
        }
        if (!_flrbt_node_is_nil(x->right)
            && _flrbt_size(x->right) < _flrbt_size(x)) {
                return 0;
        }

        return _flrbt_validate(x->left, blackBalance)
               && _flrbt_validate(x->right, blackBalance);
}

static b32 _flrbt_root_validate_(Freelist *f) {
        FreelistNode *nil = &freelist_node_nil;
        if (_flrbt_is_red(nil)) {
                return 0;
        }

        FreelistNode *x = f->rbtree;
        u64 blackNodes = 0;
        while (!_flrbt_node_is_nil(x)) {
                if (!_flrbt_is_red(x)) {
                        blackNodes++;
                }
                x = x->left;
        }
        return _flrbt_validate(f->rbtree, blackNodes);
}

static void _flrbt_left_rotate(Freelist *t, FreelistNode *x) {
        FreelistNode *y = x->right; // hr: this func assumes x.right is not nil
        x->right = y->left;
        if (!_flrbt_node_is_nil(y->left)) {
                y->left->p = x;
        }
        y->p = x->p;
        if (_flrbt_node_is_nil(x->p)) {
                t->rbtree = y;
        } else if (x == x->p->left) {
                x->p->left = y;
        } else {
                x->p->right = y;
        }
        y->left = x;
        x->p = y;
}

static void _flrbt_right_rotate(Freelist *f, FreelistNode *x) {
        FreelistNode *y = x->left; // hr: this func assumes x.left is not nil
        x->left = y->right;
        if (!_flrbt_node_is_nil(y->right)) {
                y->right->p = x;
        }
        y->p = x->p;
        if (_flrbt_node_is_nil(x->p)) {
                f->rbtree = y;
        } else if (x == x->p->left) {
                x->p->left = y;
        } else {
                x->p->right = y;
        }
        y->right = x;
        x->p = y;
}

static FreelistNode *_flrbt_tree_minimun(FreelistNode *n) {
        while (!_flrbt_node_is_nil(n->left)) {
                n = n->left;
        }
        return n;
}

static void _flrbt_insert_fixup(Freelist *f, FreelistNode *n) {
        while (_flrbt_is_red(n->p)) {
                if (n->p == n->p->p->left) {
                        FreelistNode *unc = n->p->p->right;
                        if (_flrbt_is_red(unc)) {
                                _flrbt_set_black(n->p);
                                _flrbt_set_black(unc);
                                _flrbt_set_red(n->p->p);
                                n = n->p->p;
                        } else {
                                if (n == n->p->right) {
                                        n = n->p;
                                        _flrbt_left_rotate(f, n);
                                }
                                _flrbt_set_black(n->p);
                                _flrbt_set_red(n->p->p);
                                _flrbt_right_rotate(f, n->p->p);
                        }
                } else {
                        FreelistNode *unc = n->p->p->left;
                        if (_flrbt_is_red(unc)) {
                                _flrbt_set_black(n->p);
                                _flrbt_set_black(unc);
                                _flrbt_set_red(n->p->p);
                                n = n->p->p;
                        } else {
                                if (n == n->p->left) {
                                        n = n->p;
                                        _flrbt_right_rotate(f, n);
                                }
                                _flrbt_set_black(n->p);
                                _flrbt_set_red(n->p->p);
                                _flrbt_left_rotate(f, n->p->p);
                        }
                }
        }
        _flrbt_set_black(f->rbtree);
}

static void _flrbt_insert(Freelist *f, FreelistNode *n) {
        FreelistNode *x = f->rbtree;
        FreelistNode *y = &freelist_node_nil;
        while (!_flrbt_node_is_nil(x)) {
                y = x;
                if (_flrbt_size(n) < _flrbt_size(x)) {
                        x = x->left;
                } else {
                        x = x->right;
                }
        }
        n->p = y;
        if (_flrbt_node_is_nil(y)) {
                f->rbtree = n;
        } else if (_flrbt_size(n) < _flrbt_size(y)) {
                y->left = n;
        } else {
                y->right = n;
        }
        n->left = &freelist_node_nil;
        n->right = &freelist_node_nil;
        _flrbt_set_red(n);
        _flrbt_insert_fixup(f, n);
}

static void _flrbt_transplant(Freelist *f, FreelistNode *x, FreelistNode *y) {
        if (_flrbt_node_is_nil(x->p)) {
                f->rbtree = y;
        } else if (x == x->p->left) {
                x->p->left = y;
        } else {
                x->p->right = y;
        }
        y->p = x->p; // hr: we write to y.p even when y is nil
}

static void _flrbt_delete_fixup(Freelist *f, FreelistNode *n) {
        while (n != f->rbtree && !_flrbt_is_red(n)) {
                if (n == n->p->left) {
                        FreelistNode *x = n->p->right;
                        if (_flrbt_is_red(x)) {
                                _flrbt_set_black(x);
                                _flrbt_set_red(n->p);
                                _flrbt_left_rotate(f, n->p);
                                x = n->p->right;
                        }
                        if (!_flrbt_is_red(x->left)
                            && !_flrbt_is_red(x->right)) {
                                _flrbt_set_red(x);
                                n = n->p;
                        } else {
                                if (!_flrbt_is_red(x->right)) {
                                        _flrbt_set_black(x->left);
                                        _flrbt_set_red(x);
                                        _flrbt_right_rotate(f, x);
                                        x = n->p->right;
                                }
                                _flrbt_assign_color(x, n->p);
                                _flrbt_set_black(n->p);
                                _flrbt_set_black(x->right);
                                _flrbt_left_rotate(f, n->p);
                                n = f->rbtree;
                        }
                } else {
                        FreelistNode *x = n->p->left;
                        if (_flrbt_is_red(x)) {
                                _flrbt_set_black(x);
                                _flrbt_set_red(n->p);
                                _flrbt_right_rotate(f, n->p);
                                x = n->p->left;
                        }
                        if (!_flrbt_is_red(x->right)
                            && !_flrbt_is_red(x->left)) {
                                _flrbt_set_red(x);
                                n = n->p;
                        } else {
                                if (!_flrbt_is_red(x->left)) {
                                        _flrbt_set_black(x->right);
                                        _flrbt_set_red(x);
                                        _flrbt_left_rotate(f, x);
                                        x = n->p->left;
                                }
                                _flrbt_assign_color(x, n->p);
                                _flrbt_set_black(n->p);
                                _flrbt_set_black(x->left);
                                _flrbt_right_rotate(f, n->p);
                                n = f->rbtree;
                        }
                }
        }
        _flrbt_set_black(n);
}

static void _flrbt_delete(Freelist *f, FreelistNode *n) {
        FreelistNode *x = &freelist_node_nil;
        FreelistNode *y = n;
        RBTree_Color yOriginalColor = _flrbt_is_red(y);
        if (_flrbt_node_is_nil(n->left)) {
                x = n->right;
                _flrbt_transplant(f, n, n->right);
        } else if (_flrbt_node_is_nil(n->right)) {
                x = n->left;
                _flrbt_transplant(f, n, n->left);
        } else {
                y = _flrbt_tree_minimun(n->right);
                yOriginalColor = _flrbt_is_red(y);
                x = y->right;
                if (y != n->right) {
                        _flrbt_transplant(f, y, y->right);
                        y->right = n->right;
                        y->right->p = y;
                } else {
                        x->p = y;
                }
                _flrbt_transplant(f, n, y);
                y->left = n->left;
                y->left->p = y;
                _flrbt_assign_color(y, n);
        }
        if (yOriginalColor == RBTree_Black) {
                _flrbt_delete_fixup(f, x);
        }
}

static Freelist make_freelist(u64 sizeHint) {
        Freelist res = { 0 };

        u64 size = sizeHint;
        if (!size) {
                size = mb(1);
        }
        size = align_pow2(size, 4); // hr: ensure there is room for flags

        res.mem = os_commit(size);
        if (!res.mem) {
                log_message(string8_lit(
                    "Failed to retrieve memory to create freelist\n"));
                os_abort(1);
        }
        res.size = size;

        FreelistNode *n = (FreelistNode *)res.mem;
        n->info.sizeAndFlags = size - FREELIST_HEADER_SIZE;
        n->left = &freelist_node_nil;
        n->right = &freelist_node_nil;
        n->p = &freelist_node_nil;
        _flrbt_set_black(n);
        _flrbt_set_free(n);
        res.rbtree = n;

        return res;
}

static void *freelist_alloc(Freelist *f, u64 size) {
        void *res = 0;
        size = align_pow2(size, 4); // hr: ensure there is room for flags

        // hr: find smallest section greater than or equal to size if it exists
        FreelistNode *x = f->rbtree;
        FreelistNode *y = &freelist_node_nil;
        u64 ySize = 0;
        while (!_flrbt_node_is_nil(x)) {
                u64 xSize = _flrbt_size(x);
                ySize = _flrbt_node_is_nil(y) ? UINT64_MAX : _flrbt_size(y);
                if (xSize >= size && xSize < ySize) {
                        y = x;
                }
                if (xSize == size) {
                        break;
                }
                if (xSize > size) {
                        x = x->left;
                } else {
                        x = x->right;
                }
        }
        if (_flrbt_node_is_nil(y)) {
                return res;
        }
        ySize = _flrbt_size(y);

        _flrbt_delete(f, y);

        // hr: split memory block if large enough
        u64 padding = ySize - size;
        if (padding >= 2 * FREELIST_HEADER_SIZE) {
                FreelistNode *n = ((void *)y + FREELIST_HEADER_SIZE + size);
                n->info.sizeAndFlags = padding - FREELIST_HEADER_SIZE;
                n->left = &freelist_node_nil;
                n->right = &freelist_node_nil;
                n->p = &freelist_node_nil;
                _flrbt_set_free(n);
                _flrbt_insert(f, n);
                padding = 0;
        }

        FreelistHeader *header = (FreelistHeader *)y;
        header->info.sizeAndFlags = size;
        header->padding = padding;
        _flrbt_set_allocated(header);
        res = (void *)header + FREELIST_HEADER_SIZE;

        // WARN: hr: for debugging
        if (!_flrbt_root_validate_(f)) {
                os_abort(1);
        }

        f->used += size;

        mem_zero(res, size);

        return res;
}

static void freelist_free(Freelist *f, void *ptr) {
        FreelistHeader *header = ((void *)ptr - FREELIST_HEADER_SIZE);
        f->used -= _flrbt_size(header);
        u64 size = _flrbt_size(header) + header->padding;

        // hr: merge with following block if it's free
        FreelistHeader *next = ((void *)ptr + size);
        if (_flrbt_is_free(next)) {
                FreelistNode *y = (FreelistNode *)next;
                _flrbt_delete(f, y);
                size += _flrbt_size(y) + FREELIST_HEADER_SIZE;
        }

        FreelistNode *n = (FreelistNode *)header;
        n->info.sizeAndFlags = size;
        n->left = &freelist_node_nil;
        n->right = &freelist_node_nil;
        n->p = &freelist_node_nil;
        _flrbt_set_free(n);
        _flrbt_insert(f, n);

        // WARN: hr: for debugging
        if (!_flrbt_root_validate_(f)) {
                os_abort(1);
        }
}

static b32 freelist_contains_mem(Freelist *f, void *ptr) {
        return ptr >= f->mem && ptr < f->mem + f->size;
}

static void destroy_freelist(Freelist *f) {
        os_release(f->mem, f->size);
}
