 



#include "py/mpconfig.h"





QCFG(BYTES_IN_LEN, MICROPY_QSTR_BYTES_IN_LEN)
QCFG(BYTES_IN_HASH, MICROPY_QSTR_BYTES_IN_HASH)

Q()
Q(*)
Q(_)
Q(/)
#if MICROPY_PY_SYS_PS1_PS2
Q(>>> )
Q(... )
#endif
#if MICROPY_PY_BUILTINS_STR_OP_MODULO
Q(%#o)
Q(%#x)
#else
Q({:#o})
Q({:#x})
#endif
Q({:#b})
Q( )
Q(\n)
Q(maximum recursion depth exceeded)
Q(<module>)
Q(<lambda>)
Q(<listcomp>)
Q(<dictcomp>)
Q(<setcomp>)
Q(<genexpr>)
Q(<string>)
Q(<stdin>)
Q(utf-8)

#if MICROPY_MODULE_FROZEN
Q(.frozen)
#endif

#if MICROPY_ENABLE_PYSTACK
Q(pystack exhausted)
#endif
