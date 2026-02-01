 
 

#ifndef RSEQ_COMPILER_H
#define RSEQ_COMPILER_H

 
#define rseq_after_asm_goto()	asm volatile ("" : : : "memory")

 
#define RSEQ__COMBINE_TOKENS(_tokena, _tokenb)	\
	_tokena##_tokenb
#define RSEQ_COMBINE_TOKENS(_tokena, _tokenb)	\
	RSEQ__COMBINE_TOKENS(_tokena, _tokenb)

#ifdef __cplusplus
#define rseq_unqual_scalar_typeof(x)					\
	std::remove_cv<std::remove_reference<decltype(x)>::type>::type
#else
#define rseq_scalar_type_to_expr(type)					\
	unsigned type: (unsigned type)0,				\
	signed type: (signed type)0

 
#define rseq_unqual_scalar_typeof(x)					\
	__typeof__(							\
		_Generic((x),						\
			char: (char)0,					\
			rseq_scalar_type_to_expr(char),			\
			rseq_scalar_type_to_expr(short),		\
			rseq_scalar_type_to_expr(int),			\
			rseq_scalar_type_to_expr(long),			\
			rseq_scalar_type_to_expr(long long),		\
			default: (x)					\
		)							\
	)
#endif

#endif   
