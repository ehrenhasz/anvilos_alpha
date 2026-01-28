#ifndef SHELL_MATH_H
#define SHELL_MATH_H 1
PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
#if ENABLE_FEATURE_SH_MATH_64
typedef long long arith_t;
# define ARITH_FMT "%lld"
#else
typedef long arith_t;
# define ARITH_FMT "%ld"
#endif
typedef const char* FAST_FUNC (*arith_var_lookup_t)(const char *name);
typedef void        FAST_FUNC (*arith_var_set_t)(const char *name, const char *val);
typedef struct arith_state_t {
	const char           *errmsg;
	arith_var_lookup_t    lookupvar;
	arith_var_set_t       setvar;
	void                 *list_of_recursed_names;
} arith_state_t;
arith_t FAST_FUNC arith(arith_state_t *state, const char *expr);
POP_SAVED_FUNCTION_VISIBILITY
#endif
