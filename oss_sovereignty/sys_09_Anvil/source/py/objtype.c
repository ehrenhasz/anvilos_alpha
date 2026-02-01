 

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "py/objtype.h"
#include "py/runtime.h"

#if MICROPY_DEBUG_VERBOSE 
#define DEBUG_PRINT (1)
#define DEBUG_printf DEBUG_printf
#else 
#define DEBUG_PRINT (0)
#define DEBUG_printf(...) (void)0
#endif

#define ENABLE_SPECIAL_ACCESSORS \
    (MICROPY_PY_DESCRIPTORS || MICROPY_PY_DELATTR_SETATTR || MICROPY_PY_BUILTINS_PROPERTY)

static mp_obj_t static_class_method_make_new(const mp_obj_type_t *self_in, size_t n_args, size_t n_kw, const mp_obj_t *args);

 


static int instance_count_native_bases(const mp_obj_type_t *type, const mp_obj_type_t **last_native_base) {
    int count = 0;
    for (;;) {
        if (type == &mp_type_object) {
            
            return count;
        } else if (mp_obj_is_native_type(type)) {
            
            *last_native_base = type;
            return count + 1;
        } else if (!MP_OBJ_TYPE_HAS_SLOT(type, parent)) {
            
            return count;
        #if MICROPY_MULTIPLE_INHERITANCE
        } else if (((mp_obj_base_t *)MP_OBJ_TYPE_GET_SLOT(type, parent))->type == &mp_type_tuple) {
            
            const mp_obj_tuple_t *parent_tuple = MP_OBJ_TYPE_GET_SLOT(type, parent);
            const mp_obj_t *item = parent_tuple->items;
            const mp_obj_t *top = item + parent_tuple->len;
            for (; item < top; ++item) {
                assert(mp_obj_is_type(*item, &mp_type_type));
                const mp_obj_type_t *bt = (const mp_obj_type_t *)MP_OBJ_TO_PTR(*item);
                count += instance_count_native_bases(bt, last_native_base);
            }
            return count;
        #endif
        } else {
            
            type = MP_OBJ_TYPE_GET_SLOT(type, parent);
        }
    }
}



static mp_obj_t native_base_init_wrapper(size_t n_args, const mp_obj_t *args) {
    mp_obj_instance_t *self = MP_OBJ_TO_PTR(args[0]);
    const mp_obj_type_t *native_base = NULL;
    instance_count_native_bases(self->base.type, &native_base);
    self->subobj[0] = MP_OBJ_TYPE_GET_SLOT(native_base, make_new)(native_base, n_args - 1, 0, args + 1);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(native_base_init_wrapper_obj, 1, MP_OBJ_FUN_ARGS_MAX, native_base_init_wrapper);

#if !MICROPY_CPYTHON_COMPAT
static
#endif
mp_obj_instance_t *mp_obj_new_instance(const mp_obj_type_t *class, const mp_obj_type_t **native_base) {
    size_t num_native_bases = instance_count_native_bases(class, native_base);
    assert(num_native_bases < 2);
    mp_obj_instance_t *o = mp_obj_malloc_var(mp_obj_instance_t, subobj, mp_obj_t, num_native_bases, class);
    mp_map_init(&o->members, 0);
    
    
    
    if (num_native_bases != 0) {
        o->subobj[0] = MP_OBJ_FROM_PTR(&native_base_init_wrapper_obj);
    }
    return o;
}















struct class_lookup_data {
    mp_obj_instance_t *obj;
    qstr attr;
    size_t slot_offset;
    mp_obj_t *dest;
    bool is_type;
};

static void mp_obj_class_lookup(struct class_lookup_data *lookup, const mp_obj_type_t *type) {
    assert(lookup->dest[0] == MP_OBJ_NULL);
    assert(lookup->dest[1] == MP_OBJ_NULL);
    for (;;) {
        DEBUG_printf("mp_obj_class_lookup: Looking up %s in %s\n", qstr_str(lookup->attr), qstr_str(type->name));
        
        
        
        
        if (lookup->slot_offset != 0 && mp_obj_is_native_type(type)) {
            
            
            
            if (MP_OBJ_TYPE_HAS_SLOT_BY_OFFSET(type, lookup->slot_offset) || (lookup->slot_offset == MP_OBJ_TYPE_OFFSETOF_SLOT(iter) && type->flags & MP_TYPE_FLAG_ITER_IS_STREAM)) {
                DEBUG_printf("mp_obj_class_lookup: Matched special meth slot (off=%d) for %s\n",
                    lookup->slot_offset, qstr_str(lookup->attr));
                lookup->dest[0] = MP_OBJ_SENTINEL;
                return;
            }
        }

        if (MP_OBJ_TYPE_HAS_SLOT(type, locals_dict)) {
            
            assert(mp_obj_is_dict_or_ordereddict(MP_OBJ_FROM_PTR(MP_OBJ_TYPE_GET_SLOT(type, locals_dict)))); 
            mp_map_t *locals_map = &MP_OBJ_TYPE_GET_SLOT(type, locals_dict)->map;
            mp_map_elem_t *elem = mp_map_lookup(locals_map, MP_OBJ_NEW_QSTR(lookup->attr), MP_MAP_LOOKUP);
            if (elem != NULL) {
                if (lookup->is_type) {
                    
                    
                    const mp_obj_type_t *org_type = (const mp_obj_type_t *)lookup->obj;
                    mp_convert_member_lookup(MP_OBJ_NULL, org_type, elem->value, lookup->dest);
                } else {
                    mp_obj_instance_t *obj = lookup->obj;
                    mp_obj_t obj_obj;
                    if (obj != NULL && mp_obj_is_native_type(type) && type != &mp_type_object  ) {
                        
                        obj_obj = obj->subobj[0];
                    } else {
                        obj_obj = MP_OBJ_FROM_PTR(obj);
                    }
                    mp_convert_member_lookup(obj_obj, type, elem->value, lookup->dest);
                }
                #if DEBUG_PRINT
                DEBUG_printf("mp_obj_class_lookup: Returning: ");
                mp_obj_print_helper(MICROPY_DEBUG_PRINTER, lookup->dest[0], PRINT_REPR);
                if (lookup->dest[1] != MP_OBJ_NULL) {
                    
                    DEBUG_printf(" <%s @%p>", mp_obj_get_type_str(lookup->dest[1]), MP_OBJ_TO_PTR(lookup->dest[1]));
                }
                DEBUG_printf("\n");
                #endif
                return;
            }
        }

        
        
        
        if (lookup->obj != NULL && !lookup->is_type && mp_obj_is_native_type(type) && type != &mp_type_object  ) {
            mp_load_method_maybe(lookup->obj->subobj[0], lookup->attr, lookup->dest);
            if (lookup->dest[0] != MP_OBJ_NULL) {
                return;
            }
        }

        

        if (!MP_OBJ_TYPE_HAS_SLOT(type, parent)) {
            DEBUG_printf("mp_obj_class_lookup: No more parents\n");
            return;
        #if MICROPY_MULTIPLE_INHERITANCE
        } else if (((mp_obj_base_t *)MP_OBJ_TYPE_GET_SLOT(type, parent))->type == &mp_type_tuple) {
            const mp_obj_tuple_t *parent_tuple = MP_OBJ_TYPE_GET_SLOT(type, parent);
            const mp_obj_t *item = parent_tuple->items;
            const mp_obj_t *top = item + parent_tuple->len - 1;
            for (; item < top; ++item) {
                assert(mp_obj_is_type(*item, &mp_type_type));
                mp_obj_type_t *bt = (mp_obj_type_t *)MP_OBJ_TO_PTR(*item);
                if (bt == &mp_type_object) {
                    
                    continue;
                }
                mp_obj_class_lookup(lookup, bt);
                if (lookup->dest[0] != MP_OBJ_NULL) {
                    return;
                }
            }

            
            assert(mp_obj_is_type(*item, &mp_type_type));
            type = (mp_obj_type_t *)MP_OBJ_TO_PTR(*item);
        #endif
        } else {
            type = MP_OBJ_TYPE_GET_SLOT(type, parent);
        }
        if (type == &mp_type_object) {
            
            return;
        }
    }
}

static void instance_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mp_obj_instance_t *self = MP_OBJ_TO_PTR(self_in);
    qstr meth = (kind == PRINT_STR) ? MP_QSTR___str__ : MP_QSTR___repr__;
    mp_obj_t member[2] = {MP_OBJ_NULL};
    struct class_lookup_data lookup = {
        .obj = self,
        .attr = meth,
        .slot_offset = MP_OBJ_TYPE_OFFSETOF_SLOT(print),
        .dest = member,
        .is_type = false,
    };
    mp_obj_class_lookup(&lookup, self->base.type);
    if (member[0] == MP_OBJ_NULL && kind == PRINT_STR) {
        
        lookup.attr = MP_QSTR___repr__;
        lookup.slot_offset = 0;
        mp_obj_class_lookup(&lookup, self->base.type);
    }

    if (member[0] == MP_OBJ_SENTINEL) {
        
        if (mp_obj_is_native_exception_instance(self->subobj[0])) {
            if (kind != PRINT_STR) {
                mp_print_str(print, qstr_str(self->base.type->name));
            }
            mp_obj_print_helper(print, self->subobj[0], kind | PRINT_EXC_SUBCLASS);
        } else {
            mp_obj_print_helper(print, self->subobj[0], kind);
        }
        return;
    }

    if (member[0] != MP_OBJ_NULL) {
        mp_obj_t r = mp_call_function_1(member[0], self_in);
        mp_obj_print_helper(print, r, PRINT_STR);
        return;
    }

    
    mp_printf(print, "<%s object at %p>", mp_obj_get_type_str(self_in), self);
}

static mp_obj_t mp_obj_instance_make_new(const mp_obj_type_t *self, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    assert(mp_obj_is_instance_type(self));

    
    mp_obj_t init_fn[2] = {MP_OBJ_NULL};
    struct class_lookup_data lookup = {
        .obj = NULL,
        .attr = MP_QSTR___new__,
        .slot_offset = MP_OBJ_TYPE_OFFSETOF_SLOT(make_new),
        .dest = init_fn,
        .is_type = false,
    };
    mp_obj_class_lookup(&lookup, self);

    const mp_obj_type_t *native_base = NULL;
    mp_obj_instance_t *o;
    if (init_fn[0] == MP_OBJ_NULL || init_fn[0] == MP_OBJ_SENTINEL) {
        
        
        o = mp_obj_new_instance(self, &native_base);

        
        
        
        
        

    } else {
        
        mp_obj_t new_ret;
        if (n_args == 0 && n_kw == 0) {
            mp_obj_t args2[1] = {MP_OBJ_FROM_PTR(self)};
            new_ret = mp_call_function_n_kw(init_fn[0], 1, 0, args2);
        } else {
            mp_obj_t *args2 = m_new(mp_obj_t, 1 + n_args + 2 * n_kw);
            args2[0] = MP_OBJ_FROM_PTR(self);
            memcpy(args2 + 1, args, (n_args + 2 * n_kw) * sizeof(mp_obj_t));
            new_ret = mp_call_function_n_kw(init_fn[0], n_args + 1, n_kw, args2);
            m_del(mp_obj_t, args2, 1 + n_args + 2 * n_kw);
        }

        
        
        
        if (mp_obj_get_type(new_ret) != self) {
            return new_ret;
        }

        
        o = MP_OBJ_TO_PTR(new_ret);
    }

    
    
    
    init_fn[0] = init_fn[1] = MP_OBJ_NULL;
    lookup.obj = o;
    lookup.attr = MP_QSTR___init__;
    lookup.slot_offset = 0;
    mp_obj_class_lookup(&lookup, self);
    if (init_fn[0] != MP_OBJ_NULL) {
        mp_obj_t init_ret;
        if (n_args == 0 && n_kw == 0) {
            init_ret = mp_call_method_n_kw(0, 0, init_fn);
        } else {
            mp_obj_t *args2 = m_new(mp_obj_t, 2 + n_args + 2 * n_kw);
            args2[0] = init_fn[0];
            args2[1] = init_fn[1];
            memcpy(args2 + 2, args, (n_args + 2 * n_kw) * sizeof(mp_obj_t));
            init_ret = mp_call_method_n_kw(n_args, n_kw, args2);
            m_del(mp_obj_t, args2, 2 + n_args + 2 * n_kw);
        }
        if (init_ret != mp_const_none) {
            #if MICROPY_ERROR_REPORTING <= MICROPY_ERROR_REPORTING_TERSE
            mp_raise_TypeError(MP_ERROR_TEXT("__init__() should return None"));
            #else
            mp_raise_msg_varg(&mp_type_TypeError,
                MP_ERROR_TEXT("__init__() should return None, not '%s'"), mp_obj_get_type_str(init_ret));
            #endif
        }
    }

    
    
    if (native_base != NULL && o->subobj[0] == MP_OBJ_FROM_PTR(&native_base_init_wrapper_obj)) {
        o->subobj[0] = MP_OBJ_TYPE_GET_SLOT(native_base, make_new)(native_base, n_args, n_kw, args);
    }

    return MP_OBJ_FROM_PTR(o);
}





const byte mp_unary_op_method_name[MP_UNARY_OP_NUM_RUNTIME] = {
    [MP_UNARY_OP_BOOL] = MP_QSTR___bool__,
    [MP_UNARY_OP_LEN] = MP_QSTR___len__,
    [MP_UNARY_OP_HASH] = MP_QSTR___hash__,
    [MP_UNARY_OP_INT_MAYBE] = MP_QSTR___int__,
    #if MICROPY_PY_ALL_SPECIAL_METHODS
    [MP_UNARY_OP_POSITIVE] = MP_QSTR___pos__,
    [MP_UNARY_OP_NEGATIVE] = MP_QSTR___neg__,
    [MP_UNARY_OP_INVERT] = MP_QSTR___invert__,
    [MP_UNARY_OP_ABS] = MP_QSTR___abs__,
    #endif
    #if MICROPY_PY_BUILTINS_FLOAT
    [MP_UNARY_OP_FLOAT_MAYBE] = MP_QSTR___float__,
    #if MICROPY_PY_BUILTINS_COMPLEX
    [MP_UNARY_OP_COMPLEX_MAYBE] = MP_QSTR___complex__,
    #endif
    #endif
    #if MICROPY_PY_SYS_GETSIZEOF
    [MP_UNARY_OP_SIZEOF] = MP_QSTR___sizeof__,
    #endif
};

static mp_obj_t instance_unary_op(mp_unary_op_t op, mp_obj_t self_in) {
    mp_obj_instance_t *self = MP_OBJ_TO_PTR(self_in);

    #if MICROPY_PY_SYS_GETSIZEOF
    if (MP_UNLIKELY(op == MP_UNARY_OP_SIZEOF)) {
        
        const mp_obj_type_t *native_base;
        size_t num_native_bases = instance_count_native_bases(mp_obj_get_type(self_in), &native_base);

        size_t sz = sizeof(*self) + sizeof(*self->subobj) * num_native_bases
            + sizeof(*self->members.table) * self->members.alloc;
        return MP_OBJ_NEW_SMALL_INT(sz);
    }
    #endif

    qstr op_name = mp_unary_op_method_name[op];
     
    mp_obj_t member[2] = {MP_OBJ_NULL};
    struct class_lookup_data lookup = {
        .obj = self,
        .attr = op_name,
        .slot_offset = MP_OBJ_TYPE_OFFSETOF_SLOT(unary_op),
        .dest = member,
        .is_type = false,
    };
    mp_obj_class_lookup(&lookup, self->base.type);
    if (member[0] == MP_OBJ_SENTINEL) {
        return mp_unary_op(op, self->subobj[0]);
    } else if (member[0] != MP_OBJ_NULL) {
        mp_obj_t val = mp_call_function_1(member[0], self_in);

        switch (op) {
            case MP_UNARY_OP_HASH:
                
                val = MP_OBJ_NEW_SMALL_INT(mp_obj_get_int_truncated(val));
                break;
            case MP_UNARY_OP_INT_MAYBE:
                
                if (!mp_obj_is_int(val)) {
                    mp_raise_TypeError(NULL);
                }
                break;
            default:
                
                ;
        }
        return val;
    } else {
        if (op == MP_UNARY_OP_HASH) {
            lookup.attr = MP_QSTR___eq__;
            mp_obj_class_lookup(&lookup, self->base.type);
            if (member[0] == MP_OBJ_NULL) {
                
                
                
                
                
                return MP_OBJ_NEW_SMALL_INT((mp_uint_t)self_in);
            }
            
            
        }

        return MP_OBJ_NULL; 
    }
}








const byte mp_binary_op_method_name[MP_BINARY_OP_NUM_RUNTIME] = {
    [MP_BINARY_OP_LESS] = MP_QSTR___lt__,
    [MP_BINARY_OP_MORE] = MP_QSTR___gt__,
    [MP_BINARY_OP_EQUAL] = MP_QSTR___eq__,
    [MP_BINARY_OP_LESS_EQUAL] = MP_QSTR___le__,
    [MP_BINARY_OP_MORE_EQUAL] = MP_QSTR___ge__,
    [MP_BINARY_OP_NOT_EQUAL] = MP_QSTR___ne__,
    [MP_BINARY_OP_CONTAINS] = MP_QSTR___contains__,

    
    [MP_BINARY_OP_INPLACE_ADD] = MP_QSTR___iadd__,
    [MP_BINARY_OP_INPLACE_SUBTRACT] = MP_QSTR___isub__,
    #if MICROPY_PY_ALL_INPLACE_SPECIAL_METHODS
    [MP_BINARY_OP_INPLACE_MULTIPLY] = MP_QSTR___imul__,
    [MP_BINARY_OP_INPLACE_MAT_MULTIPLY] = MP_QSTR___imatmul__,
    [MP_BINARY_OP_INPLACE_FLOOR_DIVIDE] = MP_QSTR___ifloordiv__,
    [MP_BINARY_OP_INPLACE_TRUE_DIVIDE] = MP_QSTR___itruediv__,
    [MP_BINARY_OP_INPLACE_MODULO] = MP_QSTR___imod__,
    [MP_BINARY_OP_INPLACE_POWER] = MP_QSTR___ipow__,
    [MP_BINARY_OP_INPLACE_OR] = MP_QSTR___ior__,
    [MP_BINARY_OP_INPLACE_XOR] = MP_QSTR___ixor__,
    [MP_BINARY_OP_INPLACE_AND] = MP_QSTR___iand__,
    [MP_BINARY_OP_INPLACE_LSHIFT] = MP_QSTR___ilshift__,
    [MP_BINARY_OP_INPLACE_RSHIFT] = MP_QSTR___irshift__,
    #endif

    [MP_BINARY_OP_ADD] = MP_QSTR___add__,
    [MP_BINARY_OP_SUBTRACT] = MP_QSTR___sub__,
    #if MICROPY_PY_ALL_SPECIAL_METHODS
    [MP_BINARY_OP_MULTIPLY] = MP_QSTR___mul__,
    [MP_BINARY_OP_MAT_MULTIPLY] = MP_QSTR___matmul__,
    [MP_BINARY_OP_FLOOR_DIVIDE] = MP_QSTR___floordiv__,
    [MP_BINARY_OP_TRUE_DIVIDE] = MP_QSTR___truediv__,
    [MP_BINARY_OP_MODULO] = MP_QSTR___mod__,
    [MP_BINARY_OP_DIVMOD] = MP_QSTR___divmod__,
    [MP_BINARY_OP_POWER] = MP_QSTR___pow__,
    [MP_BINARY_OP_OR] = MP_QSTR___or__,
    [MP_BINARY_OP_XOR] = MP_QSTR___xor__,
    [MP_BINARY_OP_AND] = MP_QSTR___and__,
    [MP_BINARY_OP_LSHIFT] = MP_QSTR___lshift__,
    [MP_BINARY_OP_RSHIFT] = MP_QSTR___rshift__,
    #endif

    #if MICROPY_PY_REVERSE_SPECIAL_METHODS
    [MP_BINARY_OP_REVERSE_ADD] = MP_QSTR___radd__,
    [MP_BINARY_OP_REVERSE_SUBTRACT] = MP_QSTR___rsub__,
    #if MICROPY_PY_ALL_SPECIAL_METHODS
    [MP_BINARY_OP_REVERSE_MULTIPLY] = MP_QSTR___rmul__,
    [MP_BINARY_OP_REVERSE_MAT_MULTIPLY] = MP_QSTR___rmatmul__,
    [MP_BINARY_OP_REVERSE_FLOOR_DIVIDE] = MP_QSTR___rfloordiv__,
    [MP_BINARY_OP_REVERSE_TRUE_DIVIDE] = MP_QSTR___rtruediv__,
    [MP_BINARY_OP_REVERSE_MODULO] = MP_QSTR___rmod__,
    [MP_BINARY_OP_REVERSE_POWER] = MP_QSTR___rpow__,
    [MP_BINARY_OP_REVERSE_OR] = MP_QSTR___ror__,
    [MP_BINARY_OP_REVERSE_XOR] = MP_QSTR___rxor__,
    [MP_BINARY_OP_REVERSE_AND] = MP_QSTR___rand__,
    [MP_BINARY_OP_REVERSE_LSHIFT] = MP_QSTR___rlshift__,
    [MP_BINARY_OP_REVERSE_RSHIFT] = MP_QSTR___rrshift__,
    #endif
    #endif
};

static mp_obj_t instance_binary_op(mp_binary_op_t op, mp_obj_t lhs_in, mp_obj_t rhs_in) {
    
    
    mp_obj_instance_t *lhs = MP_OBJ_TO_PTR(lhs_in);
    qstr op_name = mp_binary_op_method_name[op];
     
    mp_obj_t dest[3] = {MP_OBJ_NULL};
    struct class_lookup_data lookup = {
        .obj = lhs,
        .attr = op_name,
        .slot_offset = MP_OBJ_TYPE_OFFSETOF_SLOT(binary_op),
        .dest = dest,
        .is_type = false,
    };
    mp_obj_class_lookup(&lookup, lhs->base.type);

    mp_obj_t res;
    if (dest[0] == MP_OBJ_SENTINEL) {
        res = mp_binary_op(op, lhs->subobj[0], rhs_in);
    } else if (dest[0] != MP_OBJ_NULL) {
        dest[2] = rhs_in;
        res = mp_call_method_n_kw(1, 0, dest);
        res = op == MP_BINARY_OP_CONTAINS ? mp_obj_new_bool(mp_obj_is_true(res)) : res;
    } else {
        return MP_OBJ_NULL; 
    }

    #if MICROPY_PY_BUILTINS_NOTIMPLEMENTED
    
    
    if (res == mp_const_notimplemented) {
        return MP_OBJ_NULL; 
    }
    #endif

    return res;
}

static void mp_obj_instance_load_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    
    assert(mp_obj_is_instance_type(mp_obj_get_type(self_in)));
    mp_obj_instance_t *self = MP_OBJ_TO_PTR(self_in);

    
    mp_map_elem_t *elem = mp_map_lookup(&self->members, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP);
    if (elem != NULL) {
        
        dest[0] = elem->value;
        return;
    }
    #if MICROPY_CPYTHON_COMPAT
    if (attr == MP_QSTR___dict__) {
        
        
        mp_obj_dict_t dict;
        dict.base.type = &mp_type_dict;
        dict.map = self->members;
        dest[0] = mp_obj_dict_copy(MP_OBJ_FROM_PTR(&dict));
        mp_obj_dict_t *dest_dict = MP_OBJ_TO_PTR(dest[0]);
        dest_dict->map.is_fixed = 1;
        return;
    }
    #endif
    struct class_lookup_data lookup = {
        .obj = self,
        .attr = attr,
        .slot_offset = 0,
        .dest = dest,
        .is_type = false,
    };
    mp_obj_class_lookup(&lookup, self->base.type);
    mp_obj_t member = dest[0];
    if (member != MP_OBJ_NULL) {
        if (!(self->base.type->flags & MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS)) {
            
            return;
        }

        #if MICROPY_PY_BUILTINS_PROPERTY
        if (mp_obj_is_type(member, &mp_type_property)) {
            
            
            
            
            
            
            
            const mp_obj_t *proxy = mp_obj_property_get(member);
            if (proxy[0] == mp_const_none) {
                mp_raise_msg(&mp_type_AttributeError, MP_ERROR_TEXT("unreadable attribute"));
            } else {
                dest[0] = mp_call_function_n_kw(proxy[0], 1, 0, &self_in);
            }
            return;
        }
        #endif

        #if MICROPY_PY_DESCRIPTORS
        
        
        
        
        mp_obj_t attr_get_method[4];
        mp_load_method_maybe(member, MP_QSTR___get__, attr_get_method);
        if (attr_get_method[0] != MP_OBJ_NULL) {
            attr_get_method[2] = self_in;
            attr_get_method[3] = MP_OBJ_FROM_PTR(mp_obj_get_type(self_in));
            dest[0] = mp_call_method_n_kw(2, 0, attr_get_method);
        }
        #endif
        return;
    }

    
    if (attr != MP_QSTR___getattr__) {
        #if MICROPY_PY_DELATTR_SETATTR
        
        
        
        if (attr == MP_QSTR___setattr__ || attr == MP_QSTR___delattr__) {
            return;
        }
        #endif

        mp_obj_t dest2[3];
        mp_load_method_maybe(self_in, MP_QSTR___getattr__, dest2);
        if (dest2[0] != MP_OBJ_NULL) {
            
            dest2[2] = MP_OBJ_NEW_QSTR(attr);
            dest[0] = mp_call_method_n_kw(1, 0, dest2);
            return;
        }
    }
}

static bool mp_obj_instance_store_attr(mp_obj_t self_in, qstr attr, mp_obj_t value) {
    mp_obj_instance_t *self = MP_OBJ_TO_PTR(self_in);

    if (!(self->base.type->flags & MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS)) {
        
        goto skip_special_accessors;
    }

    #if MICROPY_PY_BUILTINS_PROPERTY || MICROPY_PY_DESCRIPTORS
    
    
    
    mp_obj_t member[2] = {MP_OBJ_NULL};
    struct class_lookup_data lookup = {
        .obj = self,
        .attr = attr,
        .slot_offset = 0,
        .dest = member,
        .is_type = false,
    };
    mp_obj_class_lookup(&lookup, self->base.type);

    if (member[0] != MP_OBJ_NULL) {
        #if MICROPY_PY_BUILTINS_PROPERTY
        if (mp_obj_is_type(member[0], &mp_type_property)) {
            
            
            
            
            
            
            
            const mp_obj_t *proxy = mp_obj_property_get(member[0]);
            mp_obj_t dest[2] = {self_in, value};
            if (value == MP_OBJ_NULL) {
                
                if (proxy[2] == mp_const_none) {
                    
                    return false;
                } else {
                    mp_call_function_n_kw(proxy[2], 1, 0, dest);
                    return true;
                }
            } else {
                
                if (proxy[1] == mp_const_none) {
                    
                    return false;
                } else {
                    mp_call_function_n_kw(proxy[1], 2, 0, dest);
                    return true;
                }
            }
        }
        #endif

        #if MICROPY_PY_DESCRIPTORS
        
        
        if (value == MP_OBJ_NULL) {
            
            mp_obj_t attr_delete_method[3];
            mp_load_method_maybe(member[0], MP_QSTR___delete__, attr_delete_method);
            if (attr_delete_method[0] != MP_OBJ_NULL) {
                attr_delete_method[2] = self_in;
                mp_call_method_n_kw(1, 0, attr_delete_method);
                return true;
            }
        } else {
            
            mp_obj_t attr_set_method[4];
            mp_load_method_maybe(member[0], MP_QSTR___set__, attr_set_method);
            if (attr_set_method[0] != MP_OBJ_NULL) {
                attr_set_method[2] = self_in;
                attr_set_method[3] = value;
                mp_call_method_n_kw(2, 0, attr_set_method);
                return true;
            }
        }
        #endif
    }
    #endif

    #if MICROPY_PY_DELATTR_SETATTR
    if (value == MP_OBJ_NULL) {
        
        
        mp_obj_t attr_delattr_method[3];
        mp_load_method_maybe(self_in, MP_QSTR___delattr__, attr_delattr_method);
        if (attr_delattr_method[0] != MP_OBJ_NULL) {
            
            attr_delattr_method[2] = MP_OBJ_NEW_QSTR(attr);
            mp_call_method_n_kw(1, 0, attr_delattr_method);
            return true;
        }
    } else {
        
        
        mp_obj_t attr_setattr_method[4];
        mp_load_method_maybe(self_in, MP_QSTR___setattr__, attr_setattr_method);
        if (attr_setattr_method[0] != MP_OBJ_NULL) {
            
            attr_setattr_method[2] = MP_OBJ_NEW_QSTR(attr);
            attr_setattr_method[3] = value;
            mp_call_method_n_kw(2, 0, attr_setattr_method);
            return true;
        }
    }
    #endif

skip_special_accessors:

    if (value == MP_OBJ_NULL) {
        
        mp_map_elem_t *elem = mp_map_lookup(&self->members, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP_REMOVE_IF_FOUND);
        return elem != NULL;
    } else {
        
        mp_map_lookup(&self->members, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP_ADD_IF_NOT_FOUND)->value = value;
        return true;
    }
}

static void mp_obj_instance_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    if (dest[0] == MP_OBJ_NULL) {
        mp_obj_instance_load_attr(self_in, attr, dest);
    } else {
        if (mp_obj_instance_store_attr(self_in, attr, dest[1])) {
            dest[0] = MP_OBJ_NULL; 
        }
    }
}

static mp_obj_t instance_subscr(mp_obj_t self_in, mp_obj_t index, mp_obj_t value) {
    mp_obj_instance_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_t member[4] = {MP_OBJ_NULL, MP_OBJ_NULL, index, value};
    struct class_lookup_data lookup = {
        .obj = self,
        .slot_offset = MP_OBJ_TYPE_OFFSETOF_SLOT(subscr),
        .dest = member,
        .is_type = false,
    };
    if (value == MP_OBJ_NULL) {
        
        lookup.attr = MP_QSTR___delitem__;
    } else if (value == MP_OBJ_SENTINEL) {
        
        lookup.attr = MP_QSTR___getitem__;
    } else {
        
        lookup.attr = MP_QSTR___setitem__;
    }
    mp_obj_class_lookup(&lookup, self->base.type);
    if (member[0] == MP_OBJ_SENTINEL) {
        return mp_obj_subscr(self->subobj[0], index, value);
    } else if (member[0] != MP_OBJ_NULL) {
        size_t n_args = value == MP_OBJ_NULL || value == MP_OBJ_SENTINEL ? 1 : 2;
        mp_obj_t ret = mp_call_method_n_kw(n_args, 0, member);
        if (value == MP_OBJ_SENTINEL) {
            return ret;
        } else {
            return mp_const_none;
        }
    } else {
        return MP_OBJ_NULL; 
    }
}

static mp_obj_t mp_obj_instance_get_call(mp_obj_t self_in, mp_obj_t *member) {
    mp_obj_instance_t *self = MP_OBJ_TO_PTR(self_in);
    struct class_lookup_data lookup = {
        .obj = self,
        .attr = MP_QSTR___call__,
        .slot_offset = MP_OBJ_TYPE_OFFSETOF_SLOT(call),
        .dest = member,
        .is_type = false,
    };
    mp_obj_class_lookup(&lookup, self->base.type);
    return member[0];
}

bool mp_obj_instance_is_callable(mp_obj_t self_in) {
    mp_obj_t member[2] = {MP_OBJ_NULL, MP_OBJ_NULL};
    return mp_obj_instance_get_call(self_in, member) != MP_OBJ_NULL;
}

mp_obj_t mp_obj_instance_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_obj_t member[2] = {MP_OBJ_NULL, MP_OBJ_NULL};
    mp_obj_t call = mp_obj_instance_get_call(self_in, member);
    if (call == MP_OBJ_NULL) {
        #if MICROPY_ERROR_REPORTING <= MICROPY_ERROR_REPORTING_TERSE
        mp_raise_TypeError(MP_ERROR_TEXT("object not callable"));
        #else
        mp_raise_msg_varg(&mp_type_TypeError,
            MP_ERROR_TEXT("'%s' object isn't callable"), mp_obj_get_type_str(self_in));
        #endif
    }
    mp_obj_instance_t *self = MP_OBJ_TO_PTR(self_in);
    if (call == MP_OBJ_SENTINEL) {
        return mp_call_function_n_kw(self->subobj[0], n_args, n_kw, args);
    }

    return mp_call_method_self_n_kw(member[0], member[1], n_args, n_kw, args);
}


mp_obj_t mp_obj_instance_getiter(mp_obj_t self_in, mp_obj_iter_buf_t *iter_buf) {
    mp_obj_instance_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_t member[2] = {MP_OBJ_NULL};
    struct class_lookup_data lookup = {
        .obj = self,
        .attr = MP_QSTR___iter__,
        .slot_offset = MP_OBJ_TYPE_OFFSETOF_SLOT(iter),
        .dest = member,
        .is_type = false,
    };
    mp_obj_class_lookup(&lookup, self->base.type);
    if (member[0] == MP_OBJ_NULL) {
        return MP_OBJ_NULL;
    } else if (member[0] == MP_OBJ_SENTINEL) {
        const mp_obj_type_t *type = mp_obj_get_type(self->subobj[0]);
        if (type->flags & MP_TYPE_FLAG_ITER_IS_ITERNEXT) {
            return self->subobj[0];
        } else {
            if (iter_buf == NULL) {
                iter_buf = m_new_obj(mp_obj_iter_buf_t);
            }
            return ((mp_getiter_fun_t)MP_OBJ_TYPE_GET_SLOT(type, iter))(self->subobj[0], iter_buf);
        }
    } else {
        return mp_call_method_n_kw(0, 0, member);
    }
}

static mp_int_t instance_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    mp_obj_instance_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_t member[2] = {MP_OBJ_NULL};
    struct class_lookup_data lookup = {
        .obj = self,
        .attr = MP_QSTR_, 
        .slot_offset = MP_OBJ_TYPE_OFFSETOF_SLOT(buffer),
        .dest = member,
        .is_type = false,
    };
    mp_obj_class_lookup(&lookup, self->base.type);
    if (member[0] == MP_OBJ_SENTINEL) {
        const mp_obj_type_t *type = mp_obj_get_type(self->subobj[0]);
        return MP_OBJ_TYPE_GET_SLOT(type, buffer)(self->subobj[0], bufinfo, flags);
    } else {
        return 1; 
    }
}

 





#if ENABLE_SPECIAL_ACCESSORS
static bool check_for_special_accessors(mp_obj_t key, mp_obj_t value) {
    #if MICROPY_PY_DELATTR_SETATTR
    if (key == MP_OBJ_NEW_QSTR(MP_QSTR___setattr__) || key == MP_OBJ_NEW_QSTR(MP_QSTR___delattr__)) {
        return true;
    }
    #endif
    #if MICROPY_PY_BUILTINS_PROPERTY
    if (mp_obj_is_type(value, &mp_type_property)) {
        return true;
    }
    #endif
    #if MICROPY_PY_DESCRIPTORS
    static const uint8_t to_check[] = {
        MP_QSTR___get__, MP_QSTR___set__, MP_QSTR___delete__,
    };
    for (size_t i = 0; i < MP_ARRAY_SIZE(to_check); ++i) {
        mp_obj_t dest_temp[2];
        mp_load_method_protected(value, to_check[i], dest_temp, true);
        if (dest_temp[0] != MP_OBJ_NULL) {
            return true;
        }
    }
    #endif
    return false;
}
#endif

static void type_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    mp_obj_type_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<class '%q'>", self->name);
}

static mp_obj_t type_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type_in;

    mp_arg_check_num(n_args, n_kw, 1, 3, false);

    switch (n_args) {
        case 1:
            return MP_OBJ_FROM_PTR(mp_obj_get_type(args[0]));

        case 3:
            
            
            
            return mp_obj_new_type(mp_obj_str_get_qstr(args[0]), args[1], args[2]);

        default:
            mp_raise_TypeError(MP_ERROR_TEXT("type takes 1 or 3 arguments"));
    }
}

static mp_obj_t type_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    

    mp_obj_type_t *self = MP_OBJ_TO_PTR(self_in);

    if (!MP_OBJ_TYPE_HAS_SLOT(self, make_new)) {
        #if MICROPY_ERROR_REPORTING <= MICROPY_ERROR_REPORTING_TERSE
        mp_raise_TypeError(MP_ERROR_TEXT("can't create instance"));
        #else
        mp_raise_msg_varg(&mp_type_TypeError, MP_ERROR_TEXT("can't create '%q' instances"), self->name);
        #endif
    }

    
    mp_obj_t o = MP_OBJ_TYPE_GET_SLOT(self, make_new)(self, n_args, n_kw, args);

    
    return o;
}

static void type_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    assert(mp_obj_is_type(self_in, &mp_type_type));
    mp_obj_type_t *self = MP_OBJ_TO_PTR(self_in);

    if (dest[0] == MP_OBJ_NULL) {
        
        #if MICROPY_CPYTHON_COMPAT
        if (attr == MP_QSTR___name__) {
            dest[0] = MP_OBJ_NEW_QSTR(self->name);
            return;
        }
        #if MICROPY_CPYTHON_COMPAT
        if (attr == MP_QSTR___dict__) {
            
            
            const mp_obj_dict_t *dict = MP_OBJ_TYPE_GET_SLOT_OR_NULL(self, locals_dict);
            if (!dict) {
                dict = &mp_const_empty_dict_obj;
            }
            if (dict->map.is_fixed) {
                dest[0] = MP_OBJ_FROM_PTR(dict);
            } else {
                dest[0] = mp_obj_dict_copy(MP_OBJ_FROM_PTR(dict));
                mp_obj_dict_t *dict_copy = MP_OBJ_TO_PTR(dest[0]);
                dict_copy->map.is_fixed = 1;
            }
            return;
        }
        #endif
        if (attr == MP_QSTR___bases__) {
            if (self == &mp_type_object) {
                dest[0] = mp_const_empty_tuple;
                return;
            }
            mp_obj_t parent_obj = MP_OBJ_TYPE_HAS_SLOT(self, parent) ? MP_OBJ_FROM_PTR(MP_OBJ_TYPE_GET_SLOT(self, parent)) : MP_OBJ_FROM_PTR(&mp_type_object);
            #if MICROPY_MULTIPLE_INHERITANCE
            if (mp_obj_is_type(parent_obj, &mp_type_tuple)) {
                dest[0] = parent_obj;
                return;
            }
            #endif
            dest[0] = mp_obj_new_tuple(1, &parent_obj);
            return;
        }
        #endif
        struct class_lookup_data lookup = {
            .obj = (mp_obj_instance_t *)self,
            .attr = attr,
            .slot_offset = 0,
            .dest = dest,
            .is_type = true,
        };
        mp_obj_class_lookup(&lookup, self);
    } else {
        

        if (MP_OBJ_TYPE_HAS_SLOT(self, locals_dict)) {
            assert(mp_obj_is_dict_or_ordereddict(MP_OBJ_FROM_PTR(MP_OBJ_TYPE_GET_SLOT(self, locals_dict)))); 
            mp_map_t *locals_map = &MP_OBJ_TYPE_GET_SLOT(self, locals_dict)->map;
            if (locals_map->is_fixed) {
                
                return;
            }
            if (dest[1] == MP_OBJ_NULL) {
                
                mp_map_elem_t *elem = mp_map_lookup(locals_map, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP_REMOVE_IF_FOUND);
                if (elem != NULL) {
                    dest[0] = MP_OBJ_NULL; 
                }
            } else {
                #if ENABLE_SPECIAL_ACCESSORS
                
                if (!(self->flags & MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS)) {
                    if (check_for_special_accessors(MP_OBJ_NEW_QSTR(attr), dest[1])) {
                        if (self->flags & MP_TYPE_FLAG_IS_SUBCLASSED) {
                            
                            mp_raise_msg(&mp_type_AttributeError, MP_ERROR_TEXT("can't add special method to already-subclassed class"));
                        }
                        self->flags |= MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS;
                    }
                }
                #endif

                
                mp_map_elem_t *elem = mp_map_lookup(locals_map, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
                elem->value = dest[1];
                dest[0] = MP_OBJ_NULL; 
            }
        }
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_type,
    MP_QSTR_type,
    MP_TYPE_FLAG_NONE,
    make_new, type_make_new,
    print, type_print,
    call, type_call,
    attr, type_attr
    );

mp_obj_t mp_obj_new_type(qstr name, mp_obj_t bases_tuple, mp_obj_t locals_dict) {
    
    if (!mp_obj_is_type(bases_tuple, &mp_type_tuple)) {
        mp_raise_TypeError(NULL);
    }
    if (!mp_obj_is_dict_or_ordereddict(locals_dict)) {
        mp_raise_TypeError(NULL);
    }

    

    
    uint16_t base_flags = MP_TYPE_FLAG_EQ_NOT_REFLEXIVE
        | MP_TYPE_FLAG_EQ_CHECKS_OTHER_TYPE
        | MP_TYPE_FLAG_EQ_HAS_NEQ_TEST
        | MP_TYPE_FLAG_ITER_IS_GETITER
        | MP_TYPE_FLAG_INSTANCE_TYPE;
    size_t bases_len;
    mp_obj_t *bases_items;
    mp_obj_tuple_get(bases_tuple, &bases_len, &bases_items);
    for (size_t i = 0; i < bases_len; i++) {
        if (!mp_obj_is_type(bases_items[i], &mp_type_type)) {
            mp_raise_TypeError(NULL);
        }
        mp_obj_type_t *t = MP_OBJ_TO_PTR(bases_items[i]);
        
        if (!MP_OBJ_TYPE_HAS_SLOT(t, make_new)) {
            #if MICROPY_ERROR_REPORTING <= MICROPY_ERROR_REPORTING_TERSE
            mp_raise_TypeError(MP_ERROR_TEXT("type isn't an acceptable base type"));
            #else
            mp_raise_msg_varg(&mp_type_TypeError,
                MP_ERROR_TEXT("type '%q' isn't an acceptable base type"), t->name);
            #endif
        }
        #if ENABLE_SPECIAL_ACCESSORS
        if (mp_obj_is_instance_type(t)) {
            t->flags |= MP_TYPE_FLAG_IS_SUBCLASSED;
            base_flags |= t->flags & MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS;
        }
        #endif
    }

    const void *base_protocol = NULL;
    if (bases_len > 0) {
        base_protocol = MP_OBJ_TYPE_GET_SLOT_OR_NULL(((mp_obj_type_t *)MP_OBJ_TO_PTR(bases_items[0])), protocol);
    }

    
    
    
    
    mp_obj_type_t *o = m_new_obj_var0(mp_obj_type_t, slots, void *, 10 + (bases_len ? 1 : 0) + (base_protocol ? 1 : 0));
    o->base.type = &mp_type_type;
    o->flags = base_flags;
    o->name = name;
    MP_OBJ_TYPE_SET_SLOT(o, make_new, mp_obj_instance_make_new, 0);
    MP_OBJ_TYPE_SET_SLOT(o, print, instance_print, 1);
    MP_OBJ_TYPE_SET_SLOT(o, call, mp_obj_instance_call, 2);
    MP_OBJ_TYPE_SET_SLOT(o, unary_op, instance_unary_op, 3);
    MP_OBJ_TYPE_SET_SLOT(o, binary_op, instance_binary_op, 4);
    MP_OBJ_TYPE_SET_SLOT(o, attr, mp_obj_instance_attr, 5);
    MP_OBJ_TYPE_SET_SLOT(o, subscr, instance_subscr, 6);
    MP_OBJ_TYPE_SET_SLOT(o, iter, mp_obj_instance_getiter, 7);
    MP_OBJ_TYPE_SET_SLOT(o, buffer, instance_get_buffer, 8);

    mp_obj_dict_t *locals_ptr = MP_OBJ_TO_PTR(locals_dict);
    MP_OBJ_TYPE_SET_SLOT(o, locals_dict, locals_ptr, 9);

    if (bases_len > 0) {
        if (bases_len >= 2) {
            #if MICROPY_MULTIPLE_INHERITANCE
            MP_OBJ_TYPE_SET_SLOT(o, parent, MP_OBJ_TO_PTR(bases_tuple), 10);
            #else
            mp_raise_NotImplementedError(MP_ERROR_TEXT("multiple inheritance not supported"));
            #endif
        } else {
            MP_OBJ_TYPE_SET_SLOT(o, parent, MP_OBJ_TO_PTR(bases_items[0]), 10);
        }

        
        
        
        
        if (base_protocol) {
            MP_OBJ_TYPE_SET_SLOT(o, protocol, base_protocol, 11);
        }
    }

    #if ENABLE_SPECIAL_ACCESSORS
    
    if (!(o->flags & MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS)) {
        for (size_t i = 0; i < locals_ptr->map.alloc; i++) {
            if (mp_map_slot_is_filled(&locals_ptr->map, i)) {
                const mp_map_elem_t *elem = &locals_ptr->map.table[i];
                if (check_for_special_accessors(elem->key, elem->value)) {
                    o->flags |= MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS;
                    break;
                }
            }
        }
    }
    #endif

    const mp_obj_type_t *native_base;
    size_t num_native_bases = instance_count_native_bases(o, &native_base);
    if (num_native_bases > 1) {
        mp_raise_TypeError(MP_ERROR_TEXT("multiple bases have instance lay-out conflict"));
    }

    mp_map_t *locals_map = &MP_OBJ_TYPE_GET_SLOT(o, locals_dict)->map;
    mp_map_elem_t *elem = mp_map_lookup(locals_map, MP_OBJ_NEW_QSTR(MP_QSTR___new__), MP_MAP_LOOKUP);
    if (elem != NULL) {
        
        if (mp_obj_is_fun(elem->value)) {
            
            elem->value = static_class_method_make_new(&mp_type_staticmethod, 1, 0, &elem->value);
        }
    }

    return MP_OBJ_FROM_PTR(o);
}

 


typedef struct _mp_obj_super_t {
    mp_obj_base_t base;
    mp_obj_t type;
    mp_obj_t obj;
} mp_obj_super_t;

static void super_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    mp_obj_super_t *self = MP_OBJ_TO_PTR(self_in);
    mp_print_str(print, "<super: ");
    mp_obj_print_helper(print, self->type, PRINT_STR);
    mp_print_str(print, ", ");
    mp_obj_print_helper(print, self->obj, PRINT_STR);
    mp_print_str(print, ">");
}

static mp_obj_t super_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type_in;
    
    
    mp_arg_check_num(n_args, n_kw, 2, 2, false);
    if (!mp_obj_is_type(args[0], &mp_type_type)) {
        mp_raise_TypeError(NULL);
    }
    mp_obj_super_t *o = m_new_obj(mp_obj_super_t);
    *o = (mp_obj_super_t) {{type_in}, args[0], args[1]};
    return MP_OBJ_FROM_PTR(o);
}

static void super_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    if (dest[0] != MP_OBJ_NULL) {
        
        return;
    }

    assert(mp_obj_is_type(self_in, &mp_type_super));
    mp_obj_super_t *self = MP_OBJ_TO_PTR(self_in);

    assert(mp_obj_is_type(self->type, &mp_type_type));

    mp_obj_type_t *type = MP_OBJ_TO_PTR(self->type);

    struct class_lookup_data lookup = {
        .obj = MP_OBJ_TO_PTR(self->obj),
        .attr = attr,
        .slot_offset = 0,
        .dest = dest,
        .is_type = false,
    };

    
    if (attr == MP_QSTR___init__) {
        lookup.slot_offset = MP_OBJ_TYPE_OFFSETOF_SLOT(make_new);
    }

    if (!MP_OBJ_TYPE_HAS_SLOT(type, parent)) {
        
    #if MICROPY_MULTIPLE_INHERITANCE
    } else if (((mp_obj_base_t *)MP_OBJ_TYPE_GET_SLOT(type, parent))->type == &mp_type_tuple) {
        const mp_obj_tuple_t *parent_tuple = MP_OBJ_TYPE_GET_SLOT(type, parent);
        size_t len = parent_tuple->len;
        const mp_obj_t *items = parent_tuple->items;
        for (size_t i = 0; i < len; i++) {
            assert(mp_obj_is_type(items[i], &mp_type_type));
            if (MP_OBJ_TO_PTR(items[i]) == &mp_type_object) {
                
                
                continue;
            }

            mp_obj_class_lookup(&lookup, (mp_obj_type_t *)MP_OBJ_TO_PTR(items[i]));
            if (dest[0] != MP_OBJ_NULL) {
                break;
            }
        }
    #endif
    } else if (MP_OBJ_TYPE_GET_SLOT(type, parent) != &mp_type_object) {
        mp_obj_class_lookup(&lookup, MP_OBJ_TYPE_GET_SLOT(type, parent));
    }

    if (dest[0] != MP_OBJ_NULL) {
        if (dest[0] == MP_OBJ_SENTINEL) {
            
            dest[0] = MP_OBJ_FROM_PTR(&native_base_init_wrapper_obj);
            dest[1] = self->obj;
        }
        return;
    }

    
    
    lookup.slot_offset = 0;

    mp_obj_class_lookup(&lookup, &mp_type_object);
}

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_super,
    MP_QSTR_super,
    MP_TYPE_FLAG_NONE,
    make_new, super_make_new,
    print, super_print,
    attr, super_attr
    );

void mp_load_super_method(qstr attr, mp_obj_t *dest) {
    mp_obj_super_t super = {{&mp_type_super}, dest[1], dest[2]};
    mp_load_method(MP_OBJ_FROM_PTR(&super), attr, dest);
}

 




bool mp_obj_is_subclass_fast(mp_const_obj_t object, mp_const_obj_t classinfo) {
    for (;;) {
        if (object == classinfo) {
            return true;
        }

        

        
        if (!mp_obj_is_type(object, &mp_type_type)) {
            return false;
        }

        const mp_obj_type_t *self = MP_OBJ_TO_PTR(object);

        if (!MP_OBJ_TYPE_HAS_SLOT(self, parent)) {
            
            return false;
        #if MICROPY_MULTIPLE_INHERITANCE
        } else if (((mp_obj_base_t *)MP_OBJ_TYPE_GET_SLOT(self, parent))->type == &mp_type_tuple) {
            
            const mp_obj_tuple_t *parent_tuple = MP_OBJ_TYPE_GET_SLOT(self, parent);
            const mp_obj_t *item = parent_tuple->items;
            const mp_obj_t *top = item + parent_tuple->len - 1;

            
            for (; item < top; ++item) {
                if (mp_obj_is_subclass_fast(*item, classinfo)) {
                    return true;
                }
            }

            
            object = *item;
        #endif
        } else {
            
            object = MP_OBJ_FROM_PTR(MP_OBJ_TYPE_GET_SLOT(self, parent));
        }
    }
}

static mp_obj_t mp_obj_is_subclass(mp_obj_t object, mp_obj_t classinfo) {
    size_t len;
    mp_obj_t *items;
    if (mp_obj_is_type(classinfo, &mp_type_type)) {
        len = 1;
        items = &classinfo;
    } else if (mp_obj_is_type(classinfo, &mp_type_tuple)) {
        mp_obj_tuple_get(classinfo, &len, &items);
    } else {
        mp_raise_TypeError(MP_ERROR_TEXT("issubclass() arg 2 must be a class or a tuple of classes"));
    }

    for (size_t i = 0; i < len; i++) {
        
        if (items[i] == MP_OBJ_FROM_PTR(&mp_type_object) || mp_obj_is_subclass_fast(object, items[i])) {
            return mp_const_true;
        }
    }
    return mp_const_false;
}

static mp_obj_t mp_builtin_issubclass(mp_obj_t object, mp_obj_t classinfo) {
    if (!mp_obj_is_type(object, &mp_type_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("issubclass() arg 1 must be a class"));
    }
    return mp_obj_is_subclass(object, classinfo);
}

MP_DEFINE_CONST_FUN_OBJ_2(mp_builtin_issubclass_obj, mp_builtin_issubclass);

static mp_obj_t mp_builtin_isinstance(mp_obj_t object, mp_obj_t classinfo) {
    return mp_obj_is_subclass(MP_OBJ_FROM_PTR(mp_obj_get_type(object)), classinfo);
}

MP_DEFINE_CONST_FUN_OBJ_2(mp_builtin_isinstance_obj, mp_builtin_isinstance);

mp_obj_t mp_obj_cast_to_native_base(mp_obj_t self_in, mp_const_obj_t native_type) {
    const mp_obj_type_t *self_type = mp_obj_get_type(self_in);

    if (MP_OBJ_FROM_PTR(self_type) == native_type) {
        return self_in;
    } else if (!mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(self_type), native_type)) {
        return MP_OBJ_NULL;
    } else {
        mp_obj_instance_t *self = (mp_obj_instance_t *)MP_OBJ_TO_PTR(self_in);
        return self->subobj[0];
    }
}

 


static mp_obj_t static_class_method_make_new(const mp_obj_type_t *self, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    assert(self == &mp_type_staticmethod || self == &mp_type_classmethod);

    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    mp_obj_static_class_method_t *o = m_new_obj(mp_obj_static_class_method_t);
    *o = (mp_obj_static_class_method_t) {{self}, args[0]};
    return MP_OBJ_FROM_PTR(o);
}

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_staticmethod,
    MP_QSTR_staticmethod,
    MP_TYPE_FLAG_NONE,
    make_new, static_class_method_make_new
    );

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_classmethod,
    MP_QSTR_classmethod,
    MP_TYPE_FLAG_NONE,
    make_new, static_class_method_make_new
    );
