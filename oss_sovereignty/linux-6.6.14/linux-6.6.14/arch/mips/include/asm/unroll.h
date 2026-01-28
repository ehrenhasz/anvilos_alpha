#ifndef __ASM_UNROLL_H__
#define __ASM_UNROLL_H__
#define unroll(times, fn, ...) do {				\
	extern void bad_unroll(void)				\
		__compiletime_error("Unsupported unroll");	\
								\
	 							\
	BUILD_BUG_ON(!__builtin_constant_p(times));		\
								\
	switch (times) {					\
	case 32: fn(__VA_ARGS__); fallthrough;			\
	case 31: fn(__VA_ARGS__); fallthrough;			\
	case 30: fn(__VA_ARGS__); fallthrough;			\
	case 29: fn(__VA_ARGS__); fallthrough;			\
	case 28: fn(__VA_ARGS__); fallthrough;			\
	case 27: fn(__VA_ARGS__); fallthrough;			\
	case 26: fn(__VA_ARGS__); fallthrough;			\
	case 25: fn(__VA_ARGS__); fallthrough;			\
	case 24: fn(__VA_ARGS__); fallthrough;			\
	case 23: fn(__VA_ARGS__); fallthrough;			\
	case 22: fn(__VA_ARGS__); fallthrough;			\
	case 21: fn(__VA_ARGS__); fallthrough;			\
	case 20: fn(__VA_ARGS__); fallthrough;			\
	case 19: fn(__VA_ARGS__); fallthrough;			\
	case 18: fn(__VA_ARGS__); fallthrough;			\
	case 17: fn(__VA_ARGS__); fallthrough;			\
	case 16: fn(__VA_ARGS__); fallthrough;			\
	case 15: fn(__VA_ARGS__); fallthrough;			\
	case 14: fn(__VA_ARGS__); fallthrough;			\
	case 13: fn(__VA_ARGS__); fallthrough;			\
	case 12: fn(__VA_ARGS__); fallthrough;			\
	case 11: fn(__VA_ARGS__); fallthrough;			\
	case 10: fn(__VA_ARGS__); fallthrough;			\
	case 9: fn(__VA_ARGS__); fallthrough;			\
	case 8: fn(__VA_ARGS__); fallthrough;			\
	case 7: fn(__VA_ARGS__); fallthrough;			\
	case 6: fn(__VA_ARGS__); fallthrough;			\
	case 5: fn(__VA_ARGS__); fallthrough;			\
	case 4: fn(__VA_ARGS__); fallthrough;			\
	case 3: fn(__VA_ARGS__); fallthrough;			\
	case 2: fn(__VA_ARGS__); fallthrough;			\
	case 1: fn(__VA_ARGS__); fallthrough;			\
	case 0: break;						\
								\
	default:						\
		 						\
		bad_unroll();					\
		break;						\
	}							\
} while (0)
#endif  
