 

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "py/mpconfig.h"
#include "py/misc.h"
#include "py/runtime.h"

#if MICROPY_DEBUG_VERBOSE 
#define DEBUG_PRINT (1)
#else 
#define DEBUG_PRINT (0)
#define DEBUG_printf(...) (void)0
#endif

#if MICROPY_OPT_MAP_LOOKUP_CACHE









#define MAP_CACHE_OFFSET(index) ((((uintptr_t)(index)) >> 2) % MICROPY_OPT_MAP_LOOKUP_CACHE_SIZE)

#define MAP_CACHE_ENTRY(index) (MP_STATE_VM(map_lookup_cache)[MAP_CACHE_OFFSET(index)])

#define MAP_CACHE_GET(map, index) (&(map)->table[MAP_CACHE_ENTRY(index) % (map)->alloc])

#define MAP_CACHE_SET(index, pos) MAP_CACHE_ENTRY(index) = (pos) & 0xff;
#else
#define MAP_CACHE_SET(index, pos)
#endif





static const uint16_t hash_allocation_sizes[] = {
    0, 2, 4, 6, 8, 10, 12, 
    17, 23, 29, 37, 47, 59, 73, 
    97, 127, 167, 223, 293, 389, 521, 691, 919, 1223, 1627, 2161, 
    3229, 4831, 7243, 10861, 16273, 24407, 36607, 54907, 
};

static size_t get_hash_alloc_greater_or_equal_to(size_t x) {
    for (size_t i = 0; i < MP_ARRAY_SIZE(hash_allocation_sizes); i++) {
        if (hash_allocation_sizes[i] >= x) {
            return hash_allocation_sizes[i];
        }
    }
    
    
    return (x + x / 2) | 1;
}

 
 

void mp_map_init(mp_map_t *map, size_t n) {
    if (n == 0) {
        map->alloc = 0;
        map->table = NULL;
    } else {
        map->alloc = n;
        map->table = m_new0(mp_map_elem_t, map->alloc);
    }
    map->used = 0;
    map->all_keys_are_qstrs = 1;
    map->is_fixed = 0;
    map->is_ordered = 0;
}

void mp_map_init_fixed_table(mp_map_t *map, size_t n, const mp_obj_t *table) {
    map->alloc = n;
    map->used = n;
    map->all_keys_are_qstrs = 1;
    map->is_fixed = 1;
    map->is_ordered = 1;
    map->table = (mp_map_elem_t *)table;
}


void mp_map_deinit(mp_map_t *map) {
    if (!map->is_fixed) {
        m_del(mp_map_elem_t, map->table, map->alloc);
    }
    map->used = map->alloc = 0;
}

void mp_map_clear(mp_map_t *map) {
    if (!map->is_fixed) {
        m_del(mp_map_elem_t, map->table, map->alloc);
    }
    map->alloc = 0;
    map->used = 0;
    map->all_keys_are_qstrs = 1;
    map->is_fixed = 0;
    map->table = NULL;
}

static void mp_map_rehash(mp_map_t *map) {
    size_t old_alloc = map->alloc;
    size_t new_alloc = get_hash_alloc_greater_or_equal_to(map->alloc + 1);
    DEBUG_printf("mp_map_rehash(%p): " UINT_FMT " -> " UINT_FMT "\n", map, old_alloc, new_alloc);
    mp_map_elem_t *old_table = map->table;
    mp_map_elem_t *new_table = m_new0(mp_map_elem_t, new_alloc);
    
    map->alloc = new_alloc;
    map->used = 0;
    map->all_keys_are_qstrs = 1;
    map->table = new_table;
    for (size_t i = 0; i < old_alloc; i++) {
        if (old_table[i].key != MP_OBJ_NULL && old_table[i].key != MP_OBJ_SENTINEL) {
            mp_map_lookup(map, old_table[i].key, MP_MAP_LOOKUP_ADD_IF_NOT_FOUND)->value = old_table[i].value;
        }
    }
    m_del(mp_map_elem_t, old_table, old_alloc);
}







mp_map_elem_t *MICROPY_WRAP_MP_MAP_LOOKUP(mp_map_lookup)(mp_map_t * map, mp_obj_t index, mp_map_lookup_kind_t lookup_kind) {
    
    assert(!map->is_fixed || lookup_kind == MP_MAP_LOOKUP);

    #if MICROPY_OPT_MAP_LOOKUP_CACHE
    
    if (lookup_kind != MP_MAP_LOOKUP_REMOVE_IF_FOUND && map->alloc) {
        mp_map_elem_t *slot = MAP_CACHE_GET(map, index);
        
        
        if (slot->key == index) {
            return slot;
        }
    }
    #endif

    
    bool compare_only_ptrs = map->all_keys_are_qstrs;
    if (compare_only_ptrs) {
        if (mp_obj_is_qstr(index)) {
            
        } else if (mp_obj_is_exact_type(index, &mp_type_str)) {
            
            
            
            
            
            compare_only_ptrs = false;
        } else if (lookup_kind != MP_MAP_LOOKUP_ADD_IF_NOT_FOUND) {
            
            
            return NULL;
        }
    }

    
    if (map->is_ordered) {
        for (mp_map_elem_t *elem = &map->table[0], *top = &map->table[map->used]; elem < top; elem++) {
            if (elem->key == index || (!compare_only_ptrs && mp_obj_equal(elem->key, index))) {
                #if MICROPY_PY_COLLECTIONS_ORDEREDDICT
                if (MP_UNLIKELY(lookup_kind == MP_MAP_LOOKUP_REMOVE_IF_FOUND)) {
                    
                    mp_obj_t value = elem->value;
                    --map->used;
                    memmove(elem, elem + 1, (top - elem - 1) * sizeof(*elem));
                    
                    
                    elem = &map->table[map->used];
                    elem->key = MP_OBJ_NULL;
                    elem->value = value;
                }
                #endif
                MAP_CACHE_SET(index, elem - map->table);
                return elem;
            }
        }
        #if MICROPY_PY_COLLECTIONS_ORDEREDDICT
        if (MP_LIKELY(lookup_kind != MP_MAP_LOOKUP_ADD_IF_NOT_FOUND)) {
            return NULL;
        }
        if (map->used == map->alloc) {
            
            map->alloc += 4;
            map->table = m_renew(mp_map_elem_t, map->table, map->used, map->alloc);
            mp_seq_clear(map->table, map->used, map->alloc, sizeof(*map->table));
        }
        mp_map_elem_t *elem = map->table + map->used++;
        elem->key = index;
        elem->value = MP_OBJ_NULL;
        if (!mp_obj_is_qstr(index)) {
            map->all_keys_are_qstrs = 0;
        }
        return elem;
        #else
        return NULL;
        #endif
    }

    

    if (map->alloc == 0) {
        if (lookup_kind == MP_MAP_LOOKUP_ADD_IF_NOT_FOUND) {
            mp_map_rehash(map);
        } else {
            return NULL;
        }
    }

    
    mp_uint_t hash;
    if (mp_obj_is_qstr(index)) {
        hash = qstr_hash(MP_OBJ_QSTR_VALUE(index));
    } else {
        hash = MP_OBJ_SMALL_INT_VALUE(mp_unary_op(MP_UNARY_OP_HASH, index));
    }

    size_t pos = hash % map->alloc;
    size_t start_pos = pos;
    mp_map_elem_t *avail_slot = NULL;
    for (;;) {
        mp_map_elem_t *slot = &map->table[pos];
        if (slot->key == MP_OBJ_NULL) {
            
            if (lookup_kind == MP_MAP_LOOKUP_ADD_IF_NOT_FOUND) {
                map->used += 1;
                if (avail_slot == NULL) {
                    avail_slot = slot;
                }
                avail_slot->key = index;
                avail_slot->value = MP_OBJ_NULL;
                if (!mp_obj_is_qstr(index)) {
                    map->all_keys_are_qstrs = 0;
                }
                return avail_slot;
            } else {
                return NULL;
            }
        } else if (slot->key == MP_OBJ_SENTINEL) {
            
            if (avail_slot == NULL) {
                avail_slot = slot;
            }
        } else if (slot->key == index || (!compare_only_ptrs && mp_obj_equal(slot->key, index))) {
            
            
            if (lookup_kind == MP_MAP_LOOKUP_REMOVE_IF_FOUND) {
                
                map->used--;
                if (map->table[(pos + 1) % map->alloc].key == MP_OBJ_NULL) {
                    
                    slot->key = MP_OBJ_NULL;
                } else {
                    slot->key = MP_OBJ_SENTINEL;
                }
                
            }
            MAP_CACHE_SET(index, pos);
            return slot;
        }

        
        pos = (pos + 1) % map->alloc;

        if (pos == start_pos) {
            
            if (lookup_kind == MP_MAP_LOOKUP_ADD_IF_NOT_FOUND) {
                if (avail_slot != NULL) {
                    
                    map->used++;
                    avail_slot->key = index;
                    avail_slot->value = MP_OBJ_NULL;
                    if (!mp_obj_is_qstr(index)) {
                        map->all_keys_are_qstrs = 0;
                    }
                    return avail_slot;
                } else {
                    
                    mp_map_rehash(map);
                    
                    start_pos = pos = hash % map->alloc;
                }
            } else {
                return NULL;
            }
        }
    }
}

 
 

#if MICROPY_PY_BUILTINS_SET

void mp_set_init(mp_set_t *set, size_t n) {
    set->alloc = n;
    set->used = 0;
    set->table = m_new0(mp_obj_t, set->alloc);
}

static void mp_set_rehash(mp_set_t *set) {
    size_t old_alloc = set->alloc;
    mp_obj_t *old_table = set->table;
    set->alloc = get_hash_alloc_greater_or_equal_to(set->alloc + 1);
    set->used = 0;
    set->table = m_new0(mp_obj_t, set->alloc);
    for (size_t i = 0; i < old_alloc; i++) {
        if (old_table[i] != MP_OBJ_NULL && old_table[i] != MP_OBJ_SENTINEL) {
            mp_set_lookup(set, old_table[i], MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
        }
    }
    m_del(mp_obj_t, old_table, old_alloc);
}

mp_obj_t mp_set_lookup(mp_set_t *set, mp_obj_t index, mp_map_lookup_kind_t lookup_kind) {
    
    

    if (set->alloc == 0) {
        if (lookup_kind & MP_MAP_LOOKUP_ADD_IF_NOT_FOUND) {
            mp_set_rehash(set);
        } else {
            return MP_OBJ_NULL;
        }
    }
    mp_uint_t hash = MP_OBJ_SMALL_INT_VALUE(mp_unary_op(MP_UNARY_OP_HASH, index));
    size_t pos = hash % set->alloc;
    size_t start_pos = pos;
    mp_obj_t *avail_slot = NULL;
    for (;;) {
        mp_obj_t elem = set->table[pos];
        if (elem == MP_OBJ_NULL) {
            
            if (lookup_kind & MP_MAP_LOOKUP_ADD_IF_NOT_FOUND) {
                if (avail_slot == NULL) {
                    avail_slot = &set->table[pos];
                }
                set->used++;
                *avail_slot = index;
                return index;
            } else {
                return MP_OBJ_NULL;
            }
        } else if (elem == MP_OBJ_SENTINEL) {
            
            if (avail_slot == NULL) {
                avail_slot = &set->table[pos];
            }
        } else if (mp_obj_equal(elem, index)) {
            
            if (lookup_kind & MP_MAP_LOOKUP_REMOVE_IF_FOUND) {
                
                set->used--;
                if (set->table[(pos + 1) % set->alloc] == MP_OBJ_NULL) {
                    
                    set->table[pos] = MP_OBJ_NULL;
                } else {
                    set->table[pos] = MP_OBJ_SENTINEL;
                }
            }
            return elem;
        }

        
        pos = (pos + 1) % set->alloc;

        if (pos == start_pos) {
            
            if (lookup_kind & MP_MAP_LOOKUP_ADD_IF_NOT_FOUND) {
                if (avail_slot != NULL) {
                    
                    set->used++;
                    *avail_slot = index;
                    return index;
                } else {
                    
                    mp_set_rehash(set);
                    
                    start_pos = pos = hash % set->alloc;
                }
            } else {
                return MP_OBJ_NULL;
            }
        }
    }
}

mp_obj_t mp_set_remove_first(mp_set_t *set) {
    for (size_t pos = 0; pos < set->alloc; pos++) {
        if (mp_set_slot_is_filled(set, pos)) {
            mp_obj_t elem = set->table[pos];
            
            set->used--;
            if (set->table[(pos + 1) % set->alloc] == MP_OBJ_NULL) {
                
                set->table[pos] = MP_OBJ_NULL;
            } else {
                set->table[pos] = MP_OBJ_SENTINEL;
            }
            return elem;
        }
    }
    return MP_OBJ_NULL;
}

void mp_set_clear(mp_set_t *set) {
    m_del(mp_obj_t, set->table, set->alloc);
    set->alloc = 0;
    set->used = 0;
    set->table = NULL;
}

#endif 

#if defined(DEBUG_PRINT) && DEBUG_PRINT
void mp_map_dump(mp_map_t *map) {
    for (size_t i = 0; i < map->alloc; i++) {
        if (map->table[i].key != MP_OBJ_NULL) {
            mp_obj_print(map->table[i].key, PRINT_REPR);
        } else {
            DEBUG_printf("(nil)");
        }
        DEBUG_printf(": %p\n", map->table[i].value);
    }
    DEBUG_printf("---\n");
}
#endif
