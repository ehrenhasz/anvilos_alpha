 
#include <stddef.h>

#include "unitypes.h"

#if @HAVE_UNISTRING_WOE32DLL_H@
# include <unistring/woe32dll.h>
#else
# define LIBUNISTRING_DLL_VARIABLE
#endif


#ifdef __cplusplus
extern "C" {
#endif


 


enum
{
  UC_DECOMP_CANONICAL, 
  UC_DECOMP_FONT,     
  UC_DECOMP_NOBREAK,  
  UC_DECOMP_INITIAL,  
  UC_DECOMP_MEDIAL,   
  UC_DECOMP_FINAL,    
  UC_DECOMP_ISOLATED, 
  UC_DECOMP_CIRCLE,   
  UC_DECOMP_SUPER,    
  UC_DECOMP_SUB,      
  UC_DECOMP_VERTICAL, 
  UC_DECOMP_WIDE,     
  UC_DECOMP_NARROW,   
  UC_DECOMP_SMALL,    
  UC_DECOMP_SQUARE,   
  UC_DECOMP_FRACTION, 
  UC_DECOMP_COMPAT    
};

 
#define UC_DECOMPOSITION_MAX_LENGTH 32

 
extern int
       uc_decomposition (ucs4_t uc, int *decomp_tag, ucs4_t *decomposition);

 
extern int
       uc_canonical_decomposition (ucs4_t uc, ucs4_t *decomposition);


 
extern ucs4_t
       uc_composition (ucs4_t uc1, ucs4_t uc2)
       _UC_ATTRIBUTE_CONST;


 
struct unicode_normalization_form;
typedef const struct unicode_normalization_form *uninorm_t;

 
extern @GNULIB_UNINORM_NFD_DLL_VARIABLE@ const struct unicode_normalization_form uninorm_nfd;
#define UNINORM_NFD (&uninorm_nfd)

 
extern @GNULIB_UNINORM_NFC_DLL_VARIABLE@ const struct unicode_normalization_form uninorm_nfc;
#define UNINORM_NFC (&uninorm_nfc)

 
extern @GNULIB_UNINORM_NFKD_DLL_VARIABLE@ const struct unicode_normalization_form uninorm_nfkd;
#define UNINORM_NFKD (&uninorm_nfkd)

 
extern @GNULIB_UNINORM_NFKC_DLL_VARIABLE@ const struct unicode_normalization_form uninorm_nfkc;
#define UNINORM_NFKC (&uninorm_nfkc)

 
#define uninorm_is_compat_decomposing(nf) \
  ((* (const unsigned int *) (nf) >> 0) & 1)

 
#define uninorm_is_composing(nf) \
  ((* (const unsigned int *) (nf) >> 1) & 1)

 
extern uninorm_t
       uninorm_decomposing_form (uninorm_t nf)
       _UC_ATTRIBUTE_PURE;


 
extern uint8_t *
       u8_normalize (uninorm_t nf, const uint8_t *s, size_t n,
                     uint8_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint16_t *
       u16_normalize (uninorm_t nf, const uint16_t *s, size_t n,
                      uint16_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint32_t *
       u32_normalize (uninorm_t nf, const uint32_t *s, size_t n,
                      uint32_t *_UC_RESTRICT resultbuf, size_t *lengthp);


 
extern int
       u8_normcmp (const uint8_t *s1, size_t n1, const uint8_t *s2, size_t n2,
                   uninorm_t nf, int *resultp);
extern int
       u16_normcmp (const uint16_t *s1, size_t n1, const uint16_t *s2, size_t n2,
                    uninorm_t nf, int *resultp);
extern int
       u32_normcmp (const uint32_t *s1, size_t n1, const uint32_t *s2, size_t n2,
                    uninorm_t nf, int *resultp);


 
extern char *
       u8_normxfrm (const uint8_t *s, size_t n, uninorm_t nf,
                    char *resultbuf, size_t *lengthp);
extern char *
       u16_normxfrm (const uint16_t *s, size_t n, uninorm_t nf,
                     char *resultbuf, size_t *lengthp);
extern char *
       u32_normxfrm (const uint32_t *s, size_t n, uninorm_t nf,
                     char *resultbuf, size_t *lengthp);


 
extern int
       u8_normcoll (const uint8_t *s1, size_t n1, const uint8_t *s2, size_t n2,
                    uninorm_t nf, int *resultp);
extern int
       u16_normcoll (const uint16_t *s1, size_t n1, const uint16_t *s2, size_t n2,
                     uninorm_t nf, int *resultp);
extern int
       u32_normcoll (const uint32_t *s1, size_t n1, const uint32_t *s2, size_t n2,
                     uninorm_t nf, int *resultp);


 

 
struct uninorm_filter;

 
extern int
       uninorm_filter_free (struct uninorm_filter *filter);

 
extern struct uninorm_filter *
       uninorm_filter_create (uninorm_t nf,
                              int (*stream_func) (void *stream_data, ucs4_t uc),
                              void *stream_data)
       _GL_ATTRIBUTE_DEALLOC (uninorm_filter_free, 1);

 
extern int
       uninorm_filter_write (struct uninorm_filter *filter, ucs4_t uc);

 
extern int
       uninorm_filter_flush (struct uninorm_filter *filter);


#ifdef __cplusplus
}
#endif


#endif  
