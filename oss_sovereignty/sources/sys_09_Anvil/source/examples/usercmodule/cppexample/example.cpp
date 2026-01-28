extern "C" {
#include <examplemodule.h>
#include <py/objstr.h>



mp_obj_t cppfunc(mp_obj_t a_obj, mp_obj_t b_obj) {
    
    
    MP_STATIC_ASSERT_STR_ARRAY_COMPATIBLE;
    if (mp_obj_is_type(a_obj, &mp_type_BaseException)) {
    }

    
    const auto a = mp_obj_get_int(a_obj);
    const auto b = mp_obj_get_int(b_obj);
    const auto sum = [&]() {
        return mp_obj_new_int(a + b);
    } ();
    
    mp_obj_t tup[] = {sum, MP_ROM_QSTR(MP_QSTR_hellocpp)};
    return mp_obj_new_tuple(2, tup);
}
}
