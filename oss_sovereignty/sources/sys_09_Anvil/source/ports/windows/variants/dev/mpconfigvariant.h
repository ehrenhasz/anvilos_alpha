

#define MICROPY_REPL_EMACS_WORDS_MOVE           (1)
#define MICROPY_REPL_EMACS_EXTRA_WORDS_MOVE     (1)
#define MICROPY_ENABLE_SCHEDULER                (1)

#define MICROPY_PY_BUILTINS_HELP                (1)
#define MICROPY_PY_BUILTINS_HELP_MODULES        (1)
#define MICROPY_PY_MATH_CONSTANTS               (1)
#define MICROPY_PY_SYS_SETTRACE                 (1)
#define MICROPY_PERSISTENT_CODE_SAVE            (1)
#define MICROPY_COMP_CONST                      (0)
#define MICROPY_PY_RANDOM_EXTRA_FUNCS           (1)
#define MICROPY_PY_BUILTINS_SLICE_INDICES       (1)
#define MICROPY_PY_SELECT                       (1)

#ifndef MICROPY_PY_ASYNCIO
#define MICROPY_PY_ASYNCIO                      (1)
#endif
