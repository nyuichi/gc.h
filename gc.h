/*
Copyright 2017 Yuichi Nishiwaki

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef _GC_H_
#define _GC_H_

#include <stddef.h>
#include "list.h"

struct gc_state {
    struct list_head spaces[2];
    struct gc_head *stack;
    char from;
} gc;

struct gc_head {
    struct list_head head;
    const struct gc_object_type *type;
    struct gc_head *stack_next;
};

struct gc_object_type {
    void (*mark)(struct gc_head *);
    void (*free)(struct gc_head *);
};

#define gc_entry(ptr, type, field) container_of(ptr, type, field)

static inline void gc_init(void) {
    INIT_LIST_HEAD(&gc.spaces[0]);
    INIT_LIST_HEAD(&gc.spaces[1]);
    gc.stack = NULL;
    gc.from = 0;
}

typedef struct gc_head *gc_scope_t;

static inline gc_scope_t gc_scope_open(void) {
    return gc.stack;
}

static inline void gc_scope_close(gc_scope_t s) {
    gc.stack = s;
}

static inline void gc_protect(struct gc_head *head) {
    head->stack_next = gc.stack;
    gc.stack = head;
}

static inline void INIT_GC_HEAD(struct gc_head *head, const struct gc_object_type *type) {
    head->type = type;
    list_add(&head->head, &gc.spaces[gc.from]);
    gc_protect(head);
}

static inline void gc_mark(struct gc_head *head) {
    if ((unsigned long) head->stack_next & 1)
        return;
    head->stack_next = (struct gc_head *) ((unsigned long) head->stack_next | 1);
    list_move_tail(&head->head, &gc.spaces[1 - gc.from]);
}

static inline void gc_run(void) {
    struct gc_head *head, *n;
    /* copy live objects */
    for (head = gc.stack; head != NULL; head = (struct gc_head *) ((unsigned long) head->stack_next & ~1ul)) {
        gc_mark(head);
    }
    list_for_each_entry (head, &gc.spaces[1 - gc.from], head) {
        if (head->type->mark)
            head->type->mark(head);
    }
    /* clean up */
    list_for_each_entry (head, &gc.spaces[1 - gc.from], head) {
        head->stack_next = (struct gc_head *) ((unsigned long) head->stack_next & ~1ul);
    }
    list_for_each_entry_safe (head, n, &gc.spaces[gc.from], head) {
        list_del(&head->head);
        if (head->type->free)
            head->type->free(head);
    }
    gc.from = 1 - gc.from;
}

static inline void gc_destroy(void) {
    gc.stack = NULL;
    gc_run();
}

#endif