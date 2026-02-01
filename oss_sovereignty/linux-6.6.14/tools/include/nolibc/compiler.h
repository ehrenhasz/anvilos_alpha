 
 
#ifndef _NOLIBC_COMPILER_H
#define _NOLIBC_COMPILER_H

#if defined(__SSP__) || defined(__SSP_STRONG__) || defined(__SSP_ALL__) || defined(__SSP_EXPLICIT__)

#define _NOLIBC_STACKPROTECTOR

#endif  

#if defined(__has_attribute)
#  if __has_attribute(no_stack_protector)
#    define __no_stack_protector __attribute__((no_stack_protector))
#  else
#    define __no_stack_protector __attribute__((__optimize__("-fno-stack-protector")))
#  endif
#else
#  define __no_stack_protector __attribute__((__optimize__("-fno-stack-protector")))
#endif  

#endif  
