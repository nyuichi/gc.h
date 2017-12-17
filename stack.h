/*-
 * stack.h - stack (singly-linked list) implementation a la list.h of Linux
 * Copyright (c) 2017 Yuichi Nishiwaki
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _STACK_H_
#define _STACK_H_

#include <stddef.h>
#include <limits.h>
#include <stdbool.h>

#undef container_of
#define container_of(ptr,type,field) ((type *) ((char *) (ptr) - offsetof(type, field)))

#undef typecheck
#define typecheck(type,var) ({ typeof(var) *__tmp; __tmp = (type *) NULL; })

// address_of(ptr, field) is equivalent to &ptr->field, except that address_of works correctly even when &ptr->field may be NULL.
#undef address_of
#define address_of(ptr,field) ((typeof(&(ptr)->field)) ((unsigned long) (ptr) + offsetof(typeof(*(ptr)), field)))

struct stack_head {
    struct stack_head *next;
};

static inline void INIT_STACK_HEAD(struct stack_head *head) {
    head->next = NULL;
}

static inline void stack_push(struct stack_head *head, struct stack_head *stack) {
    head->next = stack->next;
    stack->next = head;
}

static inline void stack_pop(struct stack_head *stack) {
    stack->next = stack->next->next;
}

static inline bool stack_empty(struct stack_head *stack) {
    return stack->next == NULL;
}

#define	stack_entry(ptr,type,field) \
    (typecheck(struct stack_head *, ptr), container_of(ptr, type, field))

#define stack_top_entry(ptr,type,field) \
    stack_entry((ptr)->next, type, field)

#define stack_for_each(p,stack) \
    for (p = (stack)->next; p; p = p->next)

#define stack_for_each_entry(p,stack,field) \
    for (p = stack_entry((stack)->next, typeof(*p), field); address_of(p, field); p = stack_entry(p->field.next, typeof(*p), field))

#define stack_for_each_entry_safe(p,n,stack,field) \
    for (p = stack_entry((stack)->next, typeof(*p), field); \
         address_of(p, field) && ((n = stack_entry(p->field.next, typeof(*n), field)), 1); \
         p = n)

#define stack_for_each_stack(p,stack) \
    for (p = (stack); p->next; p = p->next)

#define STACK_HEAD(name) struct stack_head name = { NULL }

static inline void stack_move(struct stack_head *stack, struct stack_head *head) {
    head->next = stack->next;
}

static inline void stack_move_init(struct stack_head *stack, struct stack_head *head) {
    stack_move(stack, head);
    INIT_STACK_HEAD(stack);
}

#endif