#ifndef __UAPI_ASM_BITFIELD_H
#define __UAPI_ASM_BITFIELD_H
#ifdef __MIPSEB__
#define __BITFIELD_FIELD(field, more)					\
	field;								\
	more
#elif defined(__MIPSEL__)
#define __BITFIELD_FIELD(field, more)					\
	more								\
	field;
#else  
#error "MIPS but neither __MIPSEL__ nor __MIPSEB__?"
#endif
#endif  
