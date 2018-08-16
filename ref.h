#ifndef _REF_H_
#define _REF_H_

#include <stddef.h>
#include <stdatomic.h>

#undef container_of
#define container_of(ptr,type,member) ((type *) ((char *) (ptr) - offsetof(type, member)))

#undef typecheck
#define typecheck(type,var) ({ typeof(var) *__tmp; __tmp = (type *) NULL; })

struct ref_head {
    void (*free)(struct ref_head *ref);
    _Atomic int count;
};

#define ref_entry(ref,type,field) (typecheck(struct ref_head *, ref), container_of(ref, type, field))

static inline void INIT_REF_HEAD(struct ref_head *ref, void (*free)(struct ref_head *ref)) {
    ref->free = free;
    ref->count = 1;
}

static inline void ref_inc(struct ref_head *ref) {
    if (ref != NULL)
        atomic_fetch_add(&ref->count, 1);
}

static inline void ref_dec(struct ref_head *ref) {
    if (ref != NULL)
        if (atomic_fetch_sub(&ref->count, 1) == 1)
            ref->free(ref);
}

#endif