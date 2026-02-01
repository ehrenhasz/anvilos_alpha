 

#include <wchar.h>

#if GNULIB_defined_mbstate_t

 
typedef enum
  {
    enc_other,       
    enc_utf8,        
    enc_eucjp,       
    enc_94,          
    enc_euctw,       
    enc_gb18030,     
    enc_sjis         
  }
  enc_t;

 
extern enc_t locale_encoding_classification (void);

#endif
