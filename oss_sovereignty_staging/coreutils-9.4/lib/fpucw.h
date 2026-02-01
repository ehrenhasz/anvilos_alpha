 

 

 
#if (defined __i386__ || defined __x86_64__) && (defined __GNUC__ || defined __clang__)

typedef unsigned short fpucw_t;  

# define FPU_PC_MASK 0x0300
# define FPU_PC_DOUBLE 0x200     
# define FPU_PC_EXTENDED 0x300   

# define GET_FPUCW() __extension__ \
  ({ fpucw_t _cw;                                               \
     __asm__ __volatile__ ("fnstcw %0" : "=m" (*&_cw));         \
     _cw;                                                       \
   })
# define SET_FPUCW(word) __extension__ \
  (void)({ fpucw_t _ncw = (word);                               \
           __asm__ __volatile__ ("fldcw %0" : : "m" (*&_ncw));  \
         })

# define DECL_LONG_DOUBLE_ROUNDING \
  fpucw_t oldcw;
# define BEGIN_LONG_DOUBLE_ROUNDING() \
  (void)(oldcw = GET_FPUCW (),                                  \
         SET_FPUCW ((oldcw & ~FPU_PC_MASK) | FPU_PC_EXTENDED))
# define END_LONG_DOUBLE_ROUNDING() \
  SET_FPUCW (oldcw)

#else

typedef unsigned int fpucw_t;

# define FPU_PC_MASK 0
# define FPU_PC_DOUBLE 0
# define FPU_PC_EXTENDED 0

# define GET_FPUCW() 0
# define SET_FPUCW(word) (void)(word)

# define DECL_LONG_DOUBLE_ROUNDING
# define BEGIN_LONG_DOUBLE_ROUNDING()
# define END_LONG_DOUBLE_ROUNDING()

#endif

#endif  
