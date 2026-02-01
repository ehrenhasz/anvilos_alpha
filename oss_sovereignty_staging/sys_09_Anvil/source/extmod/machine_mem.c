 

#include "py/runtime.h"
#include "extmod/modmachine.h"

#if MICROPY_PY_MACHINE_MEMX










#if !defined(MICROPY_MACHINE_MEM_GET_READ_ADDR) || !defined(MICROPY_MACHINE_MEM_GET_WRITE_ADDR)
static uintptr_t machine_mem_get_addr(mp_obj_t addr_o, uint align) {
    uintptr_t addr = mp_obj_get_int_truncated(addr_o);
    if ((addr & (align - 1)) != 0) {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("address %08x is not aligned to %d bytes"), addr, align);
    }
    return addr;
}
#if !defined(MICROPY_MACHINE_MEM_GET_READ_ADDR)
#define MICROPY_MACHINE_MEM_GET_READ_ADDR machine_mem_get_addr
#endif
#if !defined(MICROPY_MACHINE_MEM_GET_WRITE_ADDR)
#define MICROPY_MACHINE_MEM_GET_WRITE_ADDR machine_mem_get_addr
#endif
#endif

static void machine_mem_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    machine_mem_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<%u-bit memory>", 8 * self->elem_size);
}

static mp_obj_t machine_mem_subscr(mp_obj_t self_in, mp_obj_t index, mp_obj_t value) {
    
    machine_mem_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (value == MP_OBJ_NULL) {
        
        return MP_OBJ_NULL; 
    } else if (value == MP_OBJ_SENTINEL) {
        
        uintptr_t addr = MICROPY_MACHINE_MEM_GET_READ_ADDR(index, self->elem_size);
        uint32_t val;
        switch (self->elem_size) {
            case 1:
                val = (*(uint8_t *)addr);
                break;
            case 2:
                val = (*(uint16_t *)addr);
                break;
            default:
                val = (*(uint32_t *)addr);
                break;
        }
        return mp_obj_new_int(val);
    } else {
        
        uintptr_t addr = MICROPY_MACHINE_MEM_GET_WRITE_ADDR(index, self->elem_size);
        uint32_t val = mp_obj_get_int_truncated(value);
        switch (self->elem_size) {
            case 1:
                (*(uint8_t *)addr) = val;
                break;
            case 2:
                (*(uint16_t *)addr) = val;
                break;
            default:
                (*(uint32_t *)addr) = val;
                break;
        }
        return mp_const_none;
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    machine_mem_type,
    MP_QSTR_mem,
    MP_TYPE_FLAG_NONE,
    print, machine_mem_print,
    subscr, machine_mem_subscr
    );

const machine_mem_obj_t machine_mem8_obj = {{&machine_mem_type}, 1};
const machine_mem_obj_t machine_mem16_obj = {{&machine_mem_type}, 2};
const machine_mem_obj_t machine_mem32_obj = {{&machine_mem_type}, 4};

#endif 
