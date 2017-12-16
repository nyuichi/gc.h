// from picogc by Kazuho

#include <stdio.h>
#include <stdlib.h>
#include "gc.h"

struct gc_state gc;

struct list {
    int value;
    struct list *next;
    struct gc_head gc_head;
};

void list_mark(struct gc_state *gc, struct gc_head *head) {
    struct list *list = gc_entry(head, struct list, gc_head);
    if (list->next)
        gc_mark(gc, &list->next->gc_head);
}

void list_free(struct gc_state *gc, struct gc_head *head) {
    struct list *list = gc_entry(head, struct list, gc_head);
    (void) gc;
    printf("free %d!\n", list->value);
    free(list);
}

const struct gc_object_type list_type = { list_mark, list_free };

struct list *cons(int value, struct list *next) {
    struct list *list = malloc(sizeof(struct list));
    list->value = value;
    list->next = next;
    INIT_GC_HEAD(&gc, &list->gc_head, &list_type);
    gc_protect(&gc, &list->gc_head);
    return list;
}

struct list *e;

struct list *doit(void) {
    struct list *a, *b, *c, *d;
    struct gc_head *pool[5];
    struct gc_scope s;

    gc_push_scope(&gc, &s, pool);
    {
        a = cons(1, NULL);
        b = cons(2, NULL);
        c = cons(3, NULL);
        d = cons(4, a);
        e = cons(5, NULL);
        gc_pin(&gc, &e->gc_head);

        gc_run(&gc);
        puts("0 objects must be released");
    }
    gc_pop_scope(&gc);
    gc_protect(&gc, &d->gc_head);
    return d;
}

int main() {
    struct gc_head *pool[1];
    struct gc_scope scope;

    gc_init(&gc);

    gc_push_scope(&gc, &scope, pool);
    {
        struct list *list = doit();

        gc_run(&gc);
        puts("2 objects must be released");

        list->next = NULL;

        gc_run(&gc);
        puts("1 object must be released");
    }
    gc_pop_scope(&gc);

    gc_run(&gc);
    puts("1 object must be released");

    gc_unpin(&gc, &e->gc_head);

    gc_run(&gc);
    puts("1 object must be released");

    gc_destroy(&gc);
}
