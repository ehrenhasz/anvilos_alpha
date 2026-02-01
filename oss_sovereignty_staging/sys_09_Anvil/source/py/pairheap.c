 

#include "py/pairheap.h"






#define NEXT_MAKE_RIGHTMOST_PARENT(parent) ((void *)((uintptr_t)(parent) | 1))
#define NEXT_IS_RIGHTMOST_PARENT(next) ((uintptr_t)(next) & 1)
#define NEXT_GET_RIGHTMOST_PARENT(next) ((void *)((uintptr_t)(next) & ~1))


mp_pairheap_t *mp_pairheap_meld(mp_pairheap_lt_t lt, mp_pairheap_t *heap1, mp_pairheap_t *heap2) {
    if (heap1 == NULL) {
        return heap2;
    }
    if (heap2 == NULL) {
        return heap1;
    }
    if (lt(heap1, heap2)) {
        if (heap1->child == NULL) {
            heap1->child = heap2;
        } else {
            heap1->child_last->next = heap2;
        }
        heap1->child_last = heap2;
        heap2->next = NEXT_MAKE_RIGHTMOST_PARENT(heap1);
        return heap1;
    } else {
        heap1->next = heap2->child;
        heap2->child = heap1;
        if (heap1->next == NULL) {
            heap2->child_last = heap1;
            heap1->next = NEXT_MAKE_RIGHTMOST_PARENT(heap2);
        }
        return heap2;
    }
}


mp_pairheap_t *mp_pairheap_pairing(mp_pairheap_lt_t lt, mp_pairheap_t *child) {
    if (child == NULL) {
        return NULL;
    }
    mp_pairheap_t *heap = NULL;
    while (!NEXT_IS_RIGHTMOST_PARENT(child)) {
        mp_pairheap_t *n1 = child;
        child = child->next;
        n1->next = NULL;
        if (!NEXT_IS_RIGHTMOST_PARENT(child)) {
            mp_pairheap_t *n2 = child;
            child = child->next;
            n2->next = NULL;
            n1 = mp_pairheap_meld(lt, n1, n2);
        }
        heap = mp_pairheap_meld(lt, heap, n1);
    }
    heap->next = NULL;
    return heap;
}


mp_pairheap_t *mp_pairheap_delete(mp_pairheap_lt_t lt, mp_pairheap_t *heap, mp_pairheap_t *node) {
    
    if (node == heap) {
        mp_pairheap_t *child = heap->child;
        node->child = NULL;
        return mp_pairheap_pairing(lt, child);
    }

    
    if (node->next == NULL) {
        return heap;
    }

    
    mp_pairheap_t *parent = node;
    while (!NEXT_IS_RIGHTMOST_PARENT(parent->next)) {
        parent = parent->next;
    }
    parent = NEXT_GET_RIGHTMOST_PARENT(parent->next);

    
    mp_pairheap_t *next;
    if (node == parent->child && node->child == NULL) {
        if (NEXT_IS_RIGHTMOST_PARENT(node->next)) {
            parent->child = NULL;
        } else {
            parent->child = node->next;
        }
        node->next = NULL;
        return heap;
    } else if (node == parent->child) {
        mp_pairheap_t *child = node->child;
        next = node->next;
        node->child = NULL;
        node->next = NULL;
        node = mp_pairheap_pairing(lt, child);
        parent->child = node;
    } else {
        mp_pairheap_t *n = parent->child;
        while (node != n->next) {
            n = n->next;
        }
        mp_pairheap_t *child = node->child;
        next = node->next;
        node->child = NULL;
        node->next = NULL;
        node = mp_pairheap_pairing(lt, child);
        if (node == NULL) {
            node = n;
        } else {
            n->next = node;
        }
    }
    node->next = next;
    if (NEXT_IS_RIGHTMOST_PARENT(next)) {
        parent->child_last = node;
    }
    return heap;
}
