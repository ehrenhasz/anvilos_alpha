


#ifndef	_LIBNVPAIR_H
#define	_LIBNVPAIR_H extern __attribute__((visibility("default")))

#include <sys/nvpair.h>
#include <stdlib.h>
#include <stdio.h>
#include <regex.h>

#ifdef	__cplusplus
extern "C" {
#endif



_LIBNVPAIR_H int nvpair_value_match(nvpair_t *, int, const char *,
    const char **);
_LIBNVPAIR_H int nvpair_value_match_regex(nvpair_t *, int, const char *,
    regex_t *, const char **);

_LIBNVPAIR_H void nvlist_print(FILE *, nvlist_t *);
_LIBNVPAIR_H int nvlist_print_json(FILE *, nvlist_t *);
_LIBNVPAIR_H void dump_nvlist(nvlist_t *, int);



typedef struct nvlist_prtctl *nvlist_prtctl_t;	

enum nvlist_indent_mode {
	NVLIST_INDENT_ABS,	
	NVLIST_INDENT_TABBED	
};

_LIBNVPAIR_H nvlist_prtctl_t nvlist_prtctl_alloc(void);
_LIBNVPAIR_H void nvlist_prtctl_free(nvlist_prtctl_t);
_LIBNVPAIR_H void nvlist_prt(nvlist_t *, nvlist_prtctl_t);


_LIBNVPAIR_H void nvlist_prtctl_setdest(nvlist_prtctl_t, FILE *);
_LIBNVPAIR_H FILE *nvlist_prtctl_getdest(nvlist_prtctl_t);


_LIBNVPAIR_H void nvlist_prtctl_setindent(nvlist_prtctl_t,
    enum nvlist_indent_mode, int, int);
_LIBNVPAIR_H void nvlist_prtctl_doindent(nvlist_prtctl_t, int);

enum nvlist_prtctl_fmt {
	NVLIST_FMT_MEMBER_NAME,		
	NVLIST_FMT_MEMBER_POSTAMBLE,	
	NVLIST_FMT_BTWN_ARRAY		
};

_LIBNVPAIR_H void nvlist_prtctl_setfmt(nvlist_prtctl_t, enum nvlist_prtctl_fmt,
    const char *);
_LIBNVPAIR_H void nvlist_prtctl_dofmt(nvlist_prtctl_t, enum nvlist_prtctl_fmt,
    ...);



#define	NVLIST_PRINTCTL_SVDECL(funcname, valtype) \
    _LIBNVPAIR_H void funcname(nvlist_prtctl_t, \
    int (*)(nvlist_prtctl_t, void *, nvlist_t *, const char *, valtype), \
    void *)

NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_boolean, int);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_boolean_value, boolean_t);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_byte, uchar_t);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_int8, int8_t);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_uint8, uint8_t);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_int16, int16_t);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_uint16, uint16_t);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_int32, int32_t);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_uint32, uint32_t);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_int64, int64_t);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_uint64, uint64_t);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_double, double);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_string, const char *);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_hrtime, hrtime_t);
NVLIST_PRINTCTL_SVDECL(nvlist_prtctlop_nvlist, nvlist_t *);

#undef	NVLIST_PRINTCTL_SVDECL	


#define	NVLIST_PRINTCTL_AVDECL(funcname, vtype) \
    _LIBNVPAIR_H void funcname(nvlist_prtctl_t, \
    int (*)(nvlist_prtctl_t, void *, nvlist_t *, const char *, vtype, uint_t), \
    void *)

NVLIST_PRINTCTL_AVDECL(nvlist_prtctlop_boolean_array, boolean_t *);
NVLIST_PRINTCTL_AVDECL(nvlist_prtctlop_byte_array, uchar_t *);
NVLIST_PRINTCTL_AVDECL(nvlist_prtctlop_int8_array, int8_t *);
NVLIST_PRINTCTL_AVDECL(nvlist_prtctlop_uint8_array, uint8_t *);
NVLIST_PRINTCTL_AVDECL(nvlist_prtctlop_int16_array, int16_t *);
NVLIST_PRINTCTL_AVDECL(nvlist_prtctlop_uint16_array, uint16_t *);
NVLIST_PRINTCTL_AVDECL(nvlist_prtctlop_int32_array, int32_t *);
NVLIST_PRINTCTL_AVDECL(nvlist_prtctlop_uint32_array, uint32_t *);
NVLIST_PRINTCTL_AVDECL(nvlist_prtctlop_int64_array, int64_t *);
NVLIST_PRINTCTL_AVDECL(nvlist_prtctlop_uint64_array, uint64_t *);
NVLIST_PRINTCTL_AVDECL(nvlist_prtctlop_string_array, const char **);
NVLIST_PRINTCTL_AVDECL(nvlist_prtctlop_nvlist_array, nvlist_t **);

#undef	NVLIST_PRINTCTL_AVDECL	

#ifdef	__cplusplus
}
#endif

#endif	
