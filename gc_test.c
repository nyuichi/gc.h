// from picogc by Kazuho

#include <stdio.h>
#include <stdlib.h>
#include "gc.h"

struct list {
    int value;
    struct list *next;
    struct gc_head head;
};

void list_mark(struct gc_head *head) {
    struct list *list = gc_entry(head, struct list, head);
    if (list->next) {
        gc_mark(&list->next->head);
    }
}

void list_free(struct gc_head *head) {
    struct list *list = gc_entry(head, struct list, head);
    printf("free %d!\n", list->value);
    free(list);
}

const struct gc_object_type list_type = { list_mark, list_free };

struct list *cons(int value, struct list *next) {
    struct list *list = malloc(sizeof(struct list));
    INIT_GC_HEAD(&list->head, &list_type);
    list->value = value;
    list->next = next;
    return list;
}

struct list *e;

struct list *doit(void) {
    gc_scope_t s = gc_scope_open();

    struct list *a = cons(1, NULL);
    struct list *b = cons(2, NULL);
    struct list *c = cons(3, NULL);
    struct list *d = cons(4, a);
    e = cons(5, NULL);
    gc_pin(&e->head);

    gc_run();
    puts("0 objects must be released");
    
    gc_scope_close(s);
    gc_protect(&d->head);
    return d;
}

int main() {
    gc_init();

    {
        gc_scope_t s = gc_scope_open();

        struct list *list = doit();

        gc_run();
        puts("2 objects must be released");

        list->next = NULL;

        gc_run();
        puts("1 object must be released");

        gc_scope_close(s);
    }

    gc_run();
    puts("1 object must be released");

    gc_unpin(&e->head);

    gc_run();
    puts("1 object must be released");

    gc_destroy();
}
