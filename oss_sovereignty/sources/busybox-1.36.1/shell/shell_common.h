

#ifndef SHELL_COMMON_H
#define SHELL_COMMON_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

extern const char defifsvar[] ALIGN1; 
#define defifs (defifsvar + 4)

extern const char defoptindvar[] ALIGN1; 



struct builtin_read_params {
	int        read_flags;
	void FAST_FUNC (*setvar)(const char *name, const char *val);
	char       **argv;
	const char *ifs;
	const char *opt_n;
	const char *opt_p;
	const char *opt_t;
	const char *opt_u;
	const char *opt_d;
};
enum {
	BUILTIN_READ_SILENT = 1 << 0,
	BUILTIN_READ_RAW    = 1 << 1,
};





const char* FAST_FUNC
shell_builtin_read(struct builtin_read_params *params);

int FAST_FUNC
shell_builtin_ulimit(char **argv);

POP_SAVED_FUNCTION_VISIBILITY

#endif
