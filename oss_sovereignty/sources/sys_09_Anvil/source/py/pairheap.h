
#ifndef MICROPY_INCLUDED_PY_PAIRHEAP_H
#define MICROPY_INCLUDED_PY_PAIRHEAP_H










#include <assert.h>
#include "py/obj.h"






typedef struct _mp_pairheap_t {
    mp_obj_base_t base;
    struct _mp_pairheap_t *child;
    struct _mp_pairheap_t *child_last;
    struct _mp_pairheap_t *next;
} mp_pairheap_t;


typedef int (*mp_pairheap_lt_t)(mp_pairheap_t *, mp_pairheap_t *);


mp_pairheap_t *mp_pairheap_meld(mp_pairheap_lt_t lt, mp_pairheap_t *heap1, mp_pairheap_t *heap2);
mp_pairheap_t *mp_pairheap_pairing(mp_pairheap_lt_t lt, mp_pairheap_t *child);
mp_pairheap_t *mp_pairheap_delete(mp_pairheap_lt_t lt, mp_pairheap_t *heap, mp_pairheap_t *node);


static inline mp_pairheap_t *mp_pairheap_new(mp_pairheap_lt_t lt) {
    (void)lt;
    return NULL;
}


static inline void mp_pairheap_init_node(mp_pairheap_lt_t lt, mp_pairheap_t *node) {
    (void)lt;
    node->child = NULL;
    node->next = NULL;
}


static inline bool mp_pairheap_is_empty(mp_pairheap_lt_t lt, mp_pairheap_t *heap) {
    (void)lt;
    return heap == NULL;
}


static inline mp_pairheap_t *mp_pairheap_peek(mp_pairheap_lt_t lt, mp_pairheap_t *heap) {
    (void)lt;
    return heap;
}


static inline mp_pairheap_t *mp_pairheap_push(mp_pairheap_lt_t lt, mp_pairheap_t *heap, mp_pairheap_t *node) {
    assert(node->child == NULL && node->next == NULL);
    return mp_pairheap_meld(lt, node, heap); 
}


static inline mp_pairheap_t *mp_pairheap_pop(mp_pairheap_lt_t lt, mp_pairheap_t *heap) {
    assert(heap->next == NULL);
    mp_pairheap_t *child = heap->child;
    heap->child = NULL;
    return mp_pairheap_pairing(lt, child);
}

#endif 
