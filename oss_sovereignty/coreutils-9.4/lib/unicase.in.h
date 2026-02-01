 
#include <stdbool.h>

 
#include <stddef.h>

 
#include "uninorm.h"

#if @HAVE_UNISTRING_WOE32DLL_H@
# include <unistring/woe32dll.h>
#else
# define LIBUNISTRING_DLL_VARIABLE
#endif

#ifdef __cplusplus
extern "C" {
#endif

 

 

 
extern ucs4_t
       uc_toupper (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern ucs4_t
       uc_tolower (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern ucs4_t
       uc_totitle (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 

 

 

 
extern const char *
       uc_locale_language (void)
       _UC_ATTRIBUTE_PURE;

 

 
extern uint8_t *
       u8_toupper (const uint8_t *s, size_t n, const char *iso639_language,
                   uninorm_t nf,
                   uint8_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint16_t *
       u16_toupper (const uint16_t *s, size_t n, const char *iso639_language,
                    uninorm_t nf,
                    uint16_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint32_t *
       u32_toupper (const uint32_t *s, size_t n, const char *iso639_language,
                    uninorm_t nf,
                    uint32_t *_UC_RESTRICT resultbuf, size_t *lengthp);

 
extern uint8_t *
       u8_tolower (const uint8_t *s, size_t n, const char *iso639_language,
                   uninorm_t nf,
                   uint8_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint16_t *
       u16_tolower (const uint16_t *s, size_t n, const char *iso639_language,
                    uninorm_t nf,
                    uint16_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint32_t *
       u32_tolower (const uint32_t *s, size_t n, const char *iso639_language,
                    uninorm_t nf,
                    uint32_t *_UC_RESTRICT resultbuf, size_t *lengthp);

 
extern uint8_t *
       u8_totitle (const uint8_t *s, size_t n, const char *iso639_language,
                   uninorm_t nf,
                   uint8_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint16_t *
       u16_totitle (const uint16_t *s, size_t n, const char *iso639_language,
                    uninorm_t nf,
                    uint16_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint32_t *
       u32_totitle (const uint32_t *s, size_t n, const char *iso639_language,
                    uninorm_t nf,
                    uint32_t *_UC_RESTRICT resultbuf, size_t *lengthp);

 
typedef struct casing_prefix_context
        {
           
          uint32_t last_char_except_ignorable;
          uint32_t last_char_normal_or_above;
        }
        casing_prefix_context_t;
 
extern @GNULIB_UNICASE_EMPTY_PREFIX_CONTEXT_DLL_VARIABLE@ const casing_prefix_context_t unicase_empty_prefix_context;
 
extern casing_prefix_context_t
       u8_casing_prefix_context (const uint8_t *s, size_t n);
extern casing_prefix_context_t
       u16_casing_prefix_context (const uint16_t *s, size_t n);
extern casing_prefix_context_t
       u32_casing_prefix_context (const uint32_t *s, size_t n);
 
extern casing_prefix_context_t
       u8_casing_prefixes_context (const uint8_t *s, size_t n,
                                   casing_prefix_context_t a_context);
extern casing_prefix_context_t
       u16_casing_prefixes_context (const uint16_t *s, size_t n,
                                    casing_prefix_context_t a_context);
extern casing_prefix_context_t
       u32_casing_prefixes_context (const uint32_t *s, size_t n,
                                    casing_prefix_context_t a_context);

 
typedef struct casing_suffix_context
        {
           
          uint32_t first_char_except_ignorable;
          uint32_t bits;
        }
        casing_suffix_context_t;
 
extern @GNULIB_UNICASE_EMPTY_SUFFIX_CONTEXT_DLL_VARIABLE@ const casing_suffix_context_t unicase_empty_suffix_context;
 
extern casing_suffix_context_t
       u8_casing_suffix_context (const uint8_t *s, size_t n);
extern casing_suffix_context_t
       u16_casing_suffix_context (const uint16_t *s, size_t n);
extern casing_suffix_context_t
       u32_casing_suffix_context (const uint32_t *s, size_t n);
 
extern casing_suffix_context_t
       u8_casing_suffixes_context (const uint8_t *s, size_t n,
                                   casing_suffix_context_t a_context);
extern casing_suffix_context_t
       u16_casing_suffixes_context (const uint16_t *s, size_t n,
                                    casing_suffix_context_t a_context);
extern casing_suffix_context_t
       u32_casing_suffixes_context (const uint32_t *s, size_t n,
                                    casing_suffix_context_t a_context);

 
extern uint8_t *
       u8_ct_toupper (const uint8_t *s, size_t n,
                      casing_prefix_context_t prefix_context,
                      casing_suffix_context_t suffix_context,
                      const char *iso639_language,
                      uninorm_t nf,
                      uint8_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint16_t *
       u16_ct_toupper (const uint16_t *s, size_t n,
                      casing_prefix_context_t prefix_context,
                      casing_suffix_context_t suffix_context,
                      const char *iso639_language,
                      uninorm_t nf,
                      uint16_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint32_t *
       u32_ct_toupper (const uint32_t *s, size_t n,
                      casing_prefix_context_t prefix_context,
                      casing_suffix_context_t suffix_context,
                      const char *iso639_language,
                      uninorm_t nf,
                      uint32_t *_UC_RESTRICT resultbuf, size_t *lengthp);

 
extern uint8_t *
       u8_ct_tolower (const uint8_t *s, size_t n,
                      casing_prefix_context_t prefix_context,
                      casing_suffix_context_t suffix_context,
                      const char *iso639_language,
                      uninorm_t nf,
                      uint8_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint16_t *
       u16_ct_tolower (const uint16_t *s, size_t n,
                      casing_prefix_context_t prefix_context,
                      casing_suffix_context_t suffix_context,
                      const char *iso639_language,
                      uninorm_t nf,
                      uint16_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint32_t *
       u32_ct_tolower (const uint32_t *s, size_t n,
                      casing_prefix_context_t prefix_context,
                      casing_suffix_context_t suffix_context,
                      const char *iso639_language,
                      uninorm_t nf,
                      uint32_t *_UC_RESTRICT resultbuf, size_t *lengthp);

 
extern uint8_t *
       u8_ct_totitle (const uint8_t *s, size_t n,
                      casing_prefix_context_t prefix_context,
                      casing_suffix_context_t suffix_context,
                      const char *iso639_language,
                      uninorm_t nf,
                      uint8_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint16_t *
       u16_ct_totitle (const uint16_t *s, size_t n,
                      casing_prefix_context_t prefix_context,
                      casing_suffix_context_t suffix_context,
                      const char *iso639_language,
                      uninorm_t nf,
                      uint16_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint32_t *
       u32_ct_totitle (const uint32_t *s, size_t n,
                      casing_prefix_context_t prefix_context,
                      casing_suffix_context_t suffix_context,
                      const char *iso639_language,
                      uninorm_t nf,
                      uint32_t *_UC_RESTRICT resultbuf, size_t *lengthp);

 
extern uint8_t *
       u8_casefold (const uint8_t *s, size_t n, const char *iso639_language,
                    uninorm_t nf,
                    uint8_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint16_t *
       u16_casefold (const uint16_t *s, size_t n, const char *iso639_language,
                     uninorm_t nf,
                     uint16_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint32_t *
       u32_casefold (const uint32_t *s, size_t n, const char *iso639_language,
                     uninorm_t nf,
                     uint32_t *_UC_RESTRICT resultbuf, size_t *lengthp);
 
extern uint8_t *
       u8_ct_casefold (const uint8_t *s, size_t n,
                       casing_prefix_context_t prefix_context,
                       casing_suffix_context_t suffix_context,
                       const char *iso639_language,
                       uninorm_t nf,
                       uint8_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint16_t *
       u16_ct_casefold (const uint16_t *s, size_t n,
                        casing_prefix_context_t prefix_context,
                        casing_suffix_context_t suffix_context,
                        const char *iso639_language,
                        uninorm_t nf,
                        uint16_t *_UC_RESTRICT resultbuf, size_t *lengthp);
extern uint32_t *
       u32_ct_casefold (const uint32_t *s, size_t n,
                        casing_prefix_context_t prefix_context,
                        casing_suffix_context_t suffix_context,
                        const char *iso639_language,
                        uninorm_t nf,
                        uint32_t *_UC_RESTRICT resultbuf, size_t *lengthp);

 
extern int
       u8_casecmp (const uint8_t *s1, size_t n1,
                   const uint8_t *s2, size_t n2,
                   const char *iso639_language, uninorm_t nf, int *resultp);
extern int
       u16_casecmp (const uint16_t *s1, size_t n1,
                    const uint16_t *s2, size_t n2,
                    const char *iso639_language, uninorm_t nf, int *resultp);
extern int
       u32_casecmp (const uint32_t *s1, size_t n1,
                    const uint32_t *s2, size_t n2,
                    const char *iso639_language, uninorm_t nf, int *resultp);
extern int
       ulc_casecmp (const char *s1, size_t n1,
                    const char *s2, size_t n2,
                    const char *iso639_language, uninorm_t nf, int *resultp);

 
extern char *
       u8_casexfrm (const uint8_t *s, size_t n, const char *iso639_language,
                    uninorm_t nf,
                    char *_UC_RESTRICT resultbuf, size_t *lengthp);
extern char *
       u16_casexfrm (const uint16_t *s, size_t n, const char *iso639_language,
                     uninorm_t nf,
                     char *_UC_RESTRICT resultbuf, size_t *lengthp);
extern char *
       u32_casexfrm (const uint32_t *s, size_t n, const char *iso639_language,
                     uninorm_t nf,
                     char *_UC_RESTRICT resultbuf, size_t *lengthp);
extern char *
       ulc_casexfrm (const char *s, size_t n, const char *iso639_language,
                     uninorm_t nf,
                     char *_UC_RESTRICT resultbuf, size_t *lengthp);

 
extern int
       u8_casecoll (const uint8_t *s1, size_t n1,
                    const uint8_t *s2, size_t n2,
                    const char *iso639_language, uninorm_t nf, int *resultp);
extern int
       u16_casecoll (const uint16_t *s1, size_t n1,
                     const uint16_t *s2, size_t n2,
                     const char *iso639_language, uninorm_t nf, int *resultp);
extern int
       u32_casecoll (const uint32_t *s1, size_t n1,
                     const uint32_t *s2, size_t n2,
                     const char *iso639_language, uninorm_t nf, int *resultp);
extern int
       ulc_casecoll (const char *s1, size_t n1,
                     const char *s2, size_t n2,
                     const char *iso639_language, uninorm_t nf, int *resultp);


 
extern int
       u8_is_uppercase (const uint8_t *s, size_t n,
                        const char *iso639_language,
                        bool *resultp);
extern int
       u16_is_uppercase (const uint16_t *s, size_t n,
                         const char *iso639_language,
                         bool *resultp);
extern int
       u32_is_uppercase (const uint32_t *s, size_t n,
                         const char *iso639_language,
                         bool *resultp);

 
extern int
       u8_is_lowercase (const uint8_t *s, size_t n,
                        const char *iso639_language,
                        bool *resultp);
extern int
       u16_is_lowercase (const uint16_t *s, size_t n,
                         const char *iso639_language,
                         bool *resultp);
extern int
       u32_is_lowercase (const uint32_t *s, size_t n,
                         const char *iso639_language,
                         bool *resultp);

 
extern int
       u8_is_titlecase (const uint8_t *s, size_t n,
                        const char *iso639_language,
                        bool *resultp);
extern int
       u16_is_titlecase (const uint16_t *s, size_t n,
                         const char *iso639_language,
                         bool *resultp);
extern int
       u32_is_titlecase (const uint32_t *s, size_t n,
                         const char *iso639_language,
                         bool *resultp);

 
extern int
       u8_is_casefolded (const uint8_t *s, size_t n,
                         const char *iso639_language,
                         bool *resultp);
extern int
       u16_is_casefolded (const uint16_t *s, size_t n,
                          const char *iso639_language,
                          bool *resultp);
extern int
       u32_is_casefolded (const uint32_t *s, size_t n,
                          const char *iso639_language,
                          bool *resultp);

 
extern int
       u8_is_cased (const uint8_t *s, size_t n,
                    const char *iso639_language,
                    bool *resultp);
extern int
       u16_is_cased (const uint16_t *s, size_t n,
                     const char *iso639_language,
                     bool *resultp);
extern int
       u32_is_cased (const uint32_t *s, size_t n,
                     const char *iso639_language,
                     bool *resultp);


 

#ifdef __cplusplus
}
#endif

#endif  
