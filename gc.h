/*
gc.h - precise garbage collector for C

Copyright 2017 Yuichi Nishiwaki

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef GC_H
#define GC_H

#include "list.h"
#include "stack.h"

struct gc_state {
    struct list_head heap, *stage;
    struct list_head pinned, root;
    struct stack_head scope, weak_heads;
};

struct gc_head {
    struct list_head list_head;
    unsigned long type_mark;
};

struct gc_object_type {
    void (*mark)(struct gc_state *, struct gc_head *);
    void (*free)(struct gc_state *, struct gc_head *);
} __attribute__((aligned(sizeof(long))));

#define gc_entry(ptr, type, field) (typecheck(struct gc_head *, ptr), container_of(ptr, type, field))

static inline void INIT_GC_HEAD(struct gc_state *gc, struct gc_head *head, const struct gc_object_type *type) {
    INIT_LIST_HEAD(&head->list_head);
    head->type_mark = (unsigned long) type;
    list_add(&head->list_head, &gc->heap);
}

static inline const struct gc_object_type *gc_type(struct gc_head *head) {
    return (const struct gc_object_type *) (head->type_mark & ~1ul);
}

static inline void gc_mark(struct gc_state *gc, struct gc_head *head) {
    if (head->type_mark & 1)
        return;
    head->type_mark |= 1ul;
    list_move_tail(&head->list_head, gc->stage);
}

static inline void gc_del(struct gc_state *gc, struct gc_head *head) {
    list_del(&head->list_head);
    if (gc_type(head)->free) gc_type(head)->free(gc, head);
}

/* pin & unpin */

static inline void gc_pin(struct gc_state *gc, struct gc_head *head) {
    head->type_mark |= 1ul;
    list_move(&head->list_head, &gc->pinned);
}

static inline void gc_unpin(struct gc_state *gc, struct gc_head *head) {
    head->type_mark &= ~1ul;
    list_move(&head->list_head, &gc->heap);
}

/* root */

struct gc_root {
    struct list_head list_head;
    void (*mark)(struct gc_state *, struct gc_root *);
};

#define gc_root_entry(ptr, type, field) (typecheck(struct gc_root *, ptr), container_of((ptr), type, field))

static inline void gc_add_root(struct gc_state *gc, struct gc_root *root, void (*mark)(struct gc_state *, struct gc_root *)) {
    root->mark = mark;
    INIT_LIST_HEAD(&root->list_head);
    list_add(&root->list_head, &gc->root);
}

static inline void gc_del_root(struct gc_root *root) {
    list_del(&root->list_head);
}

/* scope */

struct gc_scope {
    struct gc_head **pool, **top;
    struct stack_head stack_head;
};

static inline void gc_push_scope(struct gc_state *gc, struct gc_scope *scope, struct gc_head *pool[]) {
    scope->pool = scope->top = pool;
    stack_push(&scope->stack_head, &gc->scope);
}

static inline void gc_pop_scope(struct gc_state *gc) {
    stack_pop(&gc->scope);
}

static inline void gc_protect(struct gc_state *gc, struct gc_head *head) {
    *stack_top_entry(&gc->scope, struct gc_scope, stack_head)->top++ = head;
}

/* weak */

struct gc_weak_head {
    struct gc_head gc_head, *key;
    struct stack_head stack_head, *notify;
    const struct gc_object_type *type;
};

#define gc_weak_entry(ptr, type, field) (typecheck(struct gc_weak_head *, ptr), container_of(ptr, type, field))

#define gc_weak_head_expired(head) ((head)->key == NULL)

static void gc_weak_head_mark(struct gc_state *gc, struct gc_head *head) {
    struct gc_weak_head *w = gc_entry(head, struct gc_weak_head, gc_head);
    if (gc_weak_head_expired(w)) return;
    stack_push(&w->stack_head, &gc->weak_heads);
}

static void gc_weak_head_free(struct gc_state *gc, struct gc_head *head) {
    struct gc_weak_head *w = gc_entry(head, struct gc_weak_head, gc_head);
    w->type->free(gc, head);
}

static inline void INIT_GC_WEAK_HEAD(struct gc_state *gc, struct gc_weak_head *head, const struct gc_object_type *type, struct gc_head *key, struct stack_head *notify) {
    static const struct gc_object_type weak_head_type = { gc_weak_head_mark, gc_weak_head_free };
    head->key = key;
    head->type = type;
    head->notify = notify;
    INIT_GC_HEAD(gc, &head->gc_head, &weak_head_type);
}

/* gc */

static void gc_run(struct gc_state *gc) {
    LIST_HEAD(stage);
    gc->stage = &stage;
    INIT_STACK_HEAD(&gc->weak_heads);
    /* copy objects */
    struct gc_scope *scope;
    stack_for_each_entry(scope, &gc->scope, stack_head) {
        for (struct gc_head **head = scope->pool; head != scope->top; head++)
            gc_mark(gc, *head);
    }
    struct gc_root *root;
    list_for_each_entry (root,  &gc->root, list_head) {
        root->mark(gc, root);
    }
    struct gc_head *head;
    list_for_each_entry (head, &gc->pinned, list_head) {
        if (gc_type(head)->mark) gc_type(head)->mark(gc, head);
    }
    list_for_each_entry (head, &stage, list_head) {
        if (gc_type(head)->mark) gc_type(head)->mark(gc, head);
    }
    /* deal with weak references */
    if (! stack_empty(&gc->weak_heads)) {
        struct gc_weak_head *w, *nw;
        while (1) {
            struct list_head *prev = stage.prev;
            STACK_HEAD(weak_heads);
            stack_move_init(&gc->weak_heads, &weak_heads);
            stack_for_each_entry_safe (w, nw, &weak_heads, stack_head) {
                if ((w->key->type_mark & 1ul) == 0)
                    stack_push(&w->stack_head, &gc->weak_heads);
                else
                    if (w->type->mark) w->type->mark(gc, &w->gc_head);
            }
            if (prev == stage.prev)
                break;
            list_for_each_range_entry (head, prev, &stage, list_head) {
                if (gc_type(head)->mark) gc_type(head)->mark(gc, head);
            }
        }
        stack_for_each_entry_safe (w, nw, &gc->weak_heads, stack_head) {
            w->key = NULL;
            if (w->notify)
                stack_push(&w->stack_head, w->notify);
        }
    }
    /* clean up */
    list_for_each_entry (head, &stage, list_head) {
        head->type_mark &= ~1ul;
    }
    struct gc_head *n;
    list_for_each_entry_safe (head, n, &gc->heap, list_head) {
        gc_del(gc, head);
    }
    list_splice(&stage, &gc->heap);
}

static inline void gc_init(struct gc_state *gc) {
    INIT_LIST_HEAD(&gc->heap);
    INIT_LIST_HEAD(&gc->pinned);
    INIT_LIST_HEAD(&gc->root);
    INIT_STACK_HEAD(&gc->scope);
}

static inline void gc_destroy(struct gc_state *gc) {
    list_splice_init(&gc->pinned, &gc->heap);
    INIT_LIST_HEAD(&gc->root);
    INIT_STACK_HEAD(&gc->scope);
    gc_run(gc);
}

#endif