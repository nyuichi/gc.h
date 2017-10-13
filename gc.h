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

struct gc_scope {
    struct list_head local;
    struct gc_scope *next;
};

extern struct gc_state {
    struct list_head from, to;
    struct gc_scope *scope, global;
} gc;

struct gc_head {
    struct list_head head;
    const struct gc_object_type *type;
};

struct gc_object_type {
    void (*mark)(struct gc_head *);
    void (*free)(struct gc_head *);
};

#define gc_entry(ptr, type, field) container_of(ptr, type, field)

static inline void gc_scope_open(struct gc_scope *s) {
    INIT_LIST_HEAD(&s->local);
    s->next = gc.scope;
    gc.scope = s;
}

static inline void gc_scope_close(void) {
    list_splice(&gc.scope->local, &gc.from);
    gc.scope = gc.scope->next;
}

static inline void gc_preserve(struct gc_head *head, struct gc_scope *s) {
    list_move(&head->head, &s->local);
}

static inline void gc_release(struct gc_head *head) {
    list_move(&head->head, &gc.from);
}

#define gc_protect(head) gc_preserve((head), gc.scope->next)
#define gc_pin(head) gc_preserve((head), &gc.global)

static inline void INIT_GC_HEAD(struct gc_head *head, const struct gc_object_type *type) {
    INIT_LIST_HEAD(&head->head);
    head->type = type;
    gc_preserve(head, gc.scope);
}

static inline void gc_mark(struct gc_head *head) {
    if ((unsigned long) head->type & 1)
        return;
    head->type = (struct gc_object_type *) ((unsigned long) head->type | 1);
    list_move_tail(&head->head, &gc.to);
}

#define gc_type(head) ((struct gc_object_type *) ((unsigned long) (head)->type & ~1ul))

static inline void gc_run(void) {
    struct gc_head *head, *n;
    struct gc_scope *s;
    for (s = gc.scope; s != NULL; s = s->next) {
        list_for_each_entry (head, &s->local, head)
            head->type = (struct gc_object_type *) ((unsigned long) head->type | 1);
    }
    /* copy live objects */
    INIT_LIST_HEAD(&gc.to);
    for (s = gc.scope; s != NULL; s = s->next) {
        list_for_each_entry (head, &s->local, head)
            if (gc_type(head)->mark) gc_type(head)->mark(head);
    }
    list_for_each_entry (head, &gc.to, head) {
        if (gc_type(head)->mark) gc_type(head)->mark(head);
    }
    /* clean up */
    for (s = gc.scope; s != NULL; s = s->next) {
        list_for_each_entry (head, &s->local, head)
            head->type = (struct gc_object_type *) ((unsigned long) head->type & ~1ul);
    }
    list_for_each_entry (head, &gc.to, head) {
        head->type = (struct gc_object_type *) ((unsigned long) head->type & ~1ul);
    }
    list_for_each_entry_safe (head, n, &gc.from, head) {
        if (head->type->free) head->type->free(head);
    }
    INIT_LIST_HEAD(&gc.from);
    list_splice(&gc.to, &gc.from);
}

static inline void gc_init(void) {
    INIT_LIST_HEAD(&gc.from);
    gc.scope = NULL;
    gc_scope_open(&gc.global);
}

static inline void gc_destroy(void) {
    gc.scope = NULL;
    gc_run();
}

#endif