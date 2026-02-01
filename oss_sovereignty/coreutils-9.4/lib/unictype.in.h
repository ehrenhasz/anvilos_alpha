 
#include <stdbool.h>

 
#include <stddef.h>

#if @HAVE_UNISTRING_WOE32DLL_H@
# include <unistring/woe32dll.h>
#else
# define LIBUNISTRING_DLL_VARIABLE
#endif

#ifdef __cplusplus
extern "C" {
#endif

 

 

 

 

 
typedef struct
{
  uint32_t bitmask : 31;
    unsigned int generic : 1;
  union
  {
    const void *table;                                
    bool (*lookup_fn) (ucs4_t uc, uint32_t bitmask);  
  } lookup;
}
uc_general_category_t;

 
enum
{
  UC_CATEGORY_MASK_L  = 0x0000001f,
  UC_CATEGORY_MASK_LC = 0x00000007,
  UC_CATEGORY_MASK_Lu = 0x00000001,
  UC_CATEGORY_MASK_Ll = 0x00000002,
  UC_CATEGORY_MASK_Lt = 0x00000004,
  UC_CATEGORY_MASK_Lm = 0x00000008,
  UC_CATEGORY_MASK_Lo = 0x00000010,
  UC_CATEGORY_MASK_M  = 0x000000e0,
  UC_CATEGORY_MASK_Mn = 0x00000020,
  UC_CATEGORY_MASK_Mc = 0x00000040,
  UC_CATEGORY_MASK_Me = 0x00000080,
  UC_CATEGORY_MASK_N  = 0x00000700,
  UC_CATEGORY_MASK_Nd = 0x00000100,
  UC_CATEGORY_MASK_Nl = 0x00000200,
  UC_CATEGORY_MASK_No = 0x00000400,
  UC_CATEGORY_MASK_P  = 0x0003f800,
  UC_CATEGORY_MASK_Pc = 0x00000800,
  UC_CATEGORY_MASK_Pd = 0x00001000,
  UC_CATEGORY_MASK_Ps = 0x00002000,
  UC_CATEGORY_MASK_Pe = 0x00004000,
  UC_CATEGORY_MASK_Pi = 0x00008000,
  UC_CATEGORY_MASK_Pf = 0x00010000,
  UC_CATEGORY_MASK_Po = 0x00020000,
  UC_CATEGORY_MASK_S  = 0x003c0000,
  UC_CATEGORY_MASK_Sm = 0x00040000,
  UC_CATEGORY_MASK_Sc = 0x00080000,
  UC_CATEGORY_MASK_Sk = 0x00100000,
  UC_CATEGORY_MASK_So = 0x00200000,
  UC_CATEGORY_MASK_Z  = 0x01c00000,
  UC_CATEGORY_MASK_Zs = 0x00400000,
  UC_CATEGORY_MASK_Zl = 0x00800000,
  UC_CATEGORY_MASK_Zp = 0x01000000,
  UC_CATEGORY_MASK_C  = 0x3e000000,
  UC_CATEGORY_MASK_Cc = 0x02000000,
  UC_CATEGORY_MASK_Cf = 0x04000000,
  UC_CATEGORY_MASK_Cs = 0x08000000,
  UC_CATEGORY_MASK_Co = 0x10000000,
  UC_CATEGORY_MASK_Cn = 0x20000000
};

 
extern @GNULIB_UNICTYPE_CATEGORY_L_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_L;
extern @GNULIB_UNICTYPE_CATEGORY_LC_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_LC;
extern @GNULIB_UNICTYPE_CATEGORY_LU_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Lu;
extern @GNULIB_UNICTYPE_CATEGORY_LL_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Ll;
extern @GNULIB_UNICTYPE_CATEGORY_LT_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Lt;
extern @GNULIB_UNICTYPE_CATEGORY_LM_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Lm;
extern @GNULIB_UNICTYPE_CATEGORY_LO_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Lo;
extern @GNULIB_UNICTYPE_CATEGORY_M_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_M;
extern @GNULIB_UNICTYPE_CATEGORY_MN_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Mn;
extern @GNULIB_UNICTYPE_CATEGORY_MC_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Mc;
extern @GNULIB_UNICTYPE_CATEGORY_ME_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Me;
extern @GNULIB_UNICTYPE_CATEGORY_N_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_N;
extern @GNULIB_UNICTYPE_CATEGORY_ND_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Nd;
extern @GNULIB_UNICTYPE_CATEGORY_NL_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Nl;
extern @GNULIB_UNICTYPE_CATEGORY_NO_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_No;
extern @GNULIB_UNICTYPE_CATEGORY_P_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_P;
extern @GNULIB_UNICTYPE_CATEGORY_PC_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Pc;
extern @GNULIB_UNICTYPE_CATEGORY_PD_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Pd;
extern @GNULIB_UNICTYPE_CATEGORY_PS_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Ps;
extern @GNULIB_UNICTYPE_CATEGORY_PE_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Pe;
extern @GNULIB_UNICTYPE_CATEGORY_PI_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Pi;
extern @GNULIB_UNICTYPE_CATEGORY_PF_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Pf;
extern @GNULIB_UNICTYPE_CATEGORY_PO_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Po;
extern @GNULIB_UNICTYPE_CATEGORY_S_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_S;
extern @GNULIB_UNICTYPE_CATEGORY_SM_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Sm;
extern @GNULIB_UNICTYPE_CATEGORY_SC_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Sc;
extern @GNULIB_UNICTYPE_CATEGORY_SK_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Sk;
extern @GNULIB_UNICTYPE_CATEGORY_SO_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_So;
extern @GNULIB_UNICTYPE_CATEGORY_Z_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Z;
extern @GNULIB_UNICTYPE_CATEGORY_ZS_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Zs;
extern @GNULIB_UNICTYPE_CATEGORY_ZL_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Zl;
extern @GNULIB_UNICTYPE_CATEGORY_ZP_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Zp;
extern @GNULIB_UNICTYPE_CATEGORY_C_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_C;
extern @GNULIB_UNICTYPE_CATEGORY_CC_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Cc;
extern @GNULIB_UNICTYPE_CATEGORY_CF_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Cf;
extern @GNULIB_UNICTYPE_CATEGORY_CS_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Cs;
extern @GNULIB_UNICTYPE_CATEGORY_CO_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Co;
extern @GNULIB_UNICTYPE_CATEGORY_CN_DLL_VARIABLE@ const uc_general_category_t UC_CATEGORY_Cn;
 
extern const uc_general_category_t _UC_CATEGORY_NONE;

 
#define UC_LETTER                    UC_CATEGORY_L
#define UC_CASED_LETTER              UC_CATEGORY_LC
#define UC_UPPERCASE_LETTER          UC_CATEGORY_Lu
#define UC_LOWERCASE_LETTER          UC_CATEGORY_Ll
#define UC_TITLECASE_LETTER          UC_CATEGORY_Lt
#define UC_MODIFIER_LETTER           UC_CATEGORY_Lm
#define UC_OTHER_LETTER              UC_CATEGORY_Lo
#define UC_MARK                      UC_CATEGORY_M
#define UC_NON_SPACING_MARK          UC_CATEGORY_Mn
#define UC_COMBINING_SPACING_MARK    UC_CATEGORY_Mc
#define UC_ENCLOSING_MARK            UC_CATEGORY_Me
#define UC_NUMBER                    UC_CATEGORY_N
#define UC_DECIMAL_DIGIT_NUMBER      UC_CATEGORY_Nd
#define UC_LETTER_NUMBER             UC_CATEGORY_Nl
#define UC_OTHER_NUMBER              UC_CATEGORY_No
#define UC_PUNCTUATION               UC_CATEGORY_P
#define UC_CONNECTOR_PUNCTUATION     UC_CATEGORY_Pc
#define UC_DASH_PUNCTUATION          UC_CATEGORY_Pd
#define UC_OPEN_PUNCTUATION          UC_CATEGORY_Ps  
#define UC_CLOSE_PUNCTUATION         UC_CATEGORY_Pe  
#define UC_INITIAL_QUOTE_PUNCTUATION UC_CATEGORY_Pi
#define UC_FINAL_QUOTE_PUNCTUATION   UC_CATEGORY_Pf
#define UC_OTHER_PUNCTUATION         UC_CATEGORY_Po
#define UC_SYMBOL                    UC_CATEGORY_S
#define UC_MATH_SYMBOL               UC_CATEGORY_Sm
#define UC_CURRENCY_SYMBOL           UC_CATEGORY_Sc
#define UC_MODIFIER_SYMBOL           UC_CATEGORY_Sk
#define UC_OTHER_SYMBOL              UC_CATEGORY_So
#define UC_SEPARATOR                 UC_CATEGORY_Z
#define UC_SPACE_SEPARATOR           UC_CATEGORY_Zs
#define UC_LINE_SEPARATOR            UC_CATEGORY_Zl
#define UC_PARAGRAPH_SEPARATOR       UC_CATEGORY_Zp
#define UC_OTHER                     UC_CATEGORY_C
#define UC_CONTROL                   UC_CATEGORY_Cc
#define UC_FORMAT                    UC_CATEGORY_Cf
#define UC_SURROGATE                 UC_CATEGORY_Cs  
#define UC_PRIVATE_USE               UC_CATEGORY_Co
#define UC_UNASSIGNED                UC_CATEGORY_Cn  

 
extern uc_general_category_t
       uc_general_category_or (uc_general_category_t category1,
                               uc_general_category_t category2);

 
extern uc_general_category_t
       uc_general_category_and (uc_general_category_t category1,
                                uc_general_category_t category2);

 
extern uc_general_category_t
       uc_general_category_and_not (uc_general_category_t category1,
                                    uc_general_category_t category2);

 
extern const char *
       uc_general_category_name (uc_general_category_t category)
       _UC_ATTRIBUTE_PURE;

 
extern const char *
       uc_general_category_long_name (uc_general_category_t category)
       _UC_ATTRIBUTE_PURE;

 
extern uc_general_category_t
       uc_general_category_byname (const char *category_name)
       _UC_ATTRIBUTE_PURE;

 
extern uc_general_category_t
       uc_general_category (ucs4_t uc)
       _UC_ATTRIBUTE_PURE;

 
extern bool
       uc_is_general_category (ucs4_t uc, uc_general_category_t category)
       _UC_ATTRIBUTE_PURE;
 
extern bool
       uc_is_general_category_withtable (ucs4_t uc, uint32_t bitmask)
       _UC_ATTRIBUTE_CONST;

 

 

 
enum
{
  UC_CCC_NR   =   0,  
  UC_CCC_OV   =   1,  
  UC_CCC_NK   =   7,  
  UC_CCC_KV   =   8,  
  UC_CCC_VR   =   9,  
  UC_CCC_ATBL = 200,  
  UC_CCC_ATB  = 202,  
  UC_CCC_ATA  = 214,  
  UC_CCC_ATAR = 216,  
  UC_CCC_BL   = 218,  
  UC_CCC_B    = 220,  
  UC_CCC_BR   = 222,  
  UC_CCC_L    = 224,  
  UC_CCC_R    = 226,  
  UC_CCC_AL   = 228,  
  UC_CCC_A    = 230,  
  UC_CCC_AR   = 232,  
  UC_CCC_DB   = 233,  
  UC_CCC_DA   = 234,  
  UC_CCC_IS   = 240   
};

 
extern int
       uc_combining_class (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern const char *
       uc_combining_class_name (int ccc)
       _UC_ATTRIBUTE_CONST;

 
extern const char *
       uc_combining_class_long_name (int ccc)
       _UC_ATTRIBUTE_CONST;

 
extern int
       uc_combining_class_byname (const char *ccc_name)
       _UC_ATTRIBUTE_PURE;

 

 

enum
{
  UC_BIDI_L,    
  UC_BIDI_LRE,  
  UC_BIDI_LRO,  
  UC_BIDI_R,    
  UC_BIDI_AL,   
  UC_BIDI_RLE,  
  UC_BIDI_RLO,  
  UC_BIDI_PDF,  
  UC_BIDI_EN,   
  UC_BIDI_ES,   
  UC_BIDI_ET,   
  UC_BIDI_AN,   
  UC_BIDI_CS,   
  UC_BIDI_NSM,  
  UC_BIDI_BN,   
  UC_BIDI_B,    
  UC_BIDI_S,    
  UC_BIDI_WS,   
  UC_BIDI_ON,   
  UC_BIDI_LRI,  
  UC_BIDI_RLI,  
  UC_BIDI_FSI,  
  UC_BIDI_PDI   
};

 
extern const char *
       uc_bidi_class_name (int bidi_class)
       _UC_ATTRIBUTE_CONST;
 
extern const char *
       uc_bidi_category_name (int category)
       _UC_ATTRIBUTE_CONST;

 
extern const char *
       uc_bidi_class_long_name (int bidi_class)
       _UC_ATTRIBUTE_CONST;

 
extern int
       uc_bidi_class_byname (const char *bidi_class_name)
       _UC_ATTRIBUTE_PURE;
 
extern int
       uc_bidi_category_byname (const char *category_name)
       _UC_ATTRIBUTE_PURE;

 
extern int
       uc_bidi_class (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
 
extern int
       uc_bidi_category (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_bidi_class (ucs4_t uc, int bidi_class)
       _UC_ATTRIBUTE_CONST;
 
extern bool
       uc_is_bidi_category (ucs4_t uc, int category)
       _UC_ATTRIBUTE_CONST;

 

 

 

 

 
extern int
       uc_decimal_value (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 

 

 
extern int
       uc_digit_value (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 

 

 
typedef struct
{
  int numerator;
  int denominator;
}
uc_fraction_t;
extern uc_fraction_t
       uc_numeric_value (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 

 

 
extern bool
       uc_mirror_char (ucs4_t uc, ucs4_t *puc);

 

 

 

 

 

 

 

 

 
enum
{
  UC_JOINING_TYPE_U,  
  UC_JOINING_TYPE_T,  
  UC_JOINING_TYPE_C,  
  UC_JOINING_TYPE_L,  
  UC_JOINING_TYPE_R,  
  UC_JOINING_TYPE_D   
};

 
extern const char *
       uc_joining_type_name (int joining_type)
       _UC_ATTRIBUTE_CONST;

 
extern const char *
       uc_joining_type_long_name (int joining_type)
       _UC_ATTRIBUTE_CONST;

 
extern int
       uc_joining_type_byname (const char *joining_type_name)
       _UC_ATTRIBUTE_PURE;

 
extern int
       uc_joining_type (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 

 

 
enum
{
  UC_JOINING_GROUP_NONE,                      
  UC_JOINING_GROUP_AIN,                       
  UC_JOINING_GROUP_ALAPH,                     
  UC_JOINING_GROUP_ALEF,                      
  UC_JOINING_GROUP_BEH,                       
  UC_JOINING_GROUP_BETH,                      
  UC_JOINING_GROUP_BURUSHASKI_YEH_BARREE,     
  UC_JOINING_GROUP_DAL,                       
  UC_JOINING_GROUP_DALATH_RISH,               
  UC_JOINING_GROUP_E,                         
  UC_JOINING_GROUP_FARSI_YEH,                 
  UC_JOINING_GROUP_FE,                        
  UC_JOINING_GROUP_FEH,                       
  UC_JOINING_GROUP_FINAL_SEMKATH,             
  UC_JOINING_GROUP_GAF,                       
  UC_JOINING_GROUP_GAMAL,                     
  UC_JOINING_GROUP_HAH,                       
  UC_JOINING_GROUP_HE,                        
  UC_JOINING_GROUP_HEH,                       
  UC_JOINING_GROUP_HEH_GOAL,                  
  UC_JOINING_GROUP_HETH,                      
  UC_JOINING_GROUP_KAF,                       
  UC_JOINING_GROUP_KAPH,                      
  UC_JOINING_GROUP_KHAPH,                     
  UC_JOINING_GROUP_KNOTTED_HEH,               
  UC_JOINING_GROUP_LAM,                       
  UC_JOINING_GROUP_LAMADH,                    
  UC_JOINING_GROUP_MEEM,                      
  UC_JOINING_GROUP_MIM,                       
  UC_JOINING_GROUP_NOON,                      
  UC_JOINING_GROUP_NUN,                       
  UC_JOINING_GROUP_NYA,                       
  UC_JOINING_GROUP_PE,                        
  UC_JOINING_GROUP_QAF,                       
  UC_JOINING_GROUP_QAPH,                      
  UC_JOINING_GROUP_REH,                       
  UC_JOINING_GROUP_REVERSED_PE,               
  UC_JOINING_GROUP_SAD,                       
  UC_JOINING_GROUP_SADHE,                     
  UC_JOINING_GROUP_SEEN,                      
  UC_JOINING_GROUP_SEMKATH,                   
  UC_JOINING_GROUP_SHIN,                      
  UC_JOINING_GROUP_SWASH_KAF,                 
  UC_JOINING_GROUP_SYRIAC_WAW,                
  UC_JOINING_GROUP_TAH,                       
  UC_JOINING_GROUP_TAW,                       
  UC_JOINING_GROUP_TEH_MARBUTA,               
  UC_JOINING_GROUP_TEH_MARBUTA_GOAL,          
  UC_JOINING_GROUP_TETH,                      
  UC_JOINING_GROUP_WAW,                       
  UC_JOINING_GROUP_YEH,                       
  UC_JOINING_GROUP_YEH_BARREE,                
  UC_JOINING_GROUP_YEH_WITH_TAIL,             
  UC_JOINING_GROUP_YUDH,                      
  UC_JOINING_GROUP_YUDH_HE,                   
  UC_JOINING_GROUP_ZAIN,                      
  UC_JOINING_GROUP_ZHAIN,                     
  UC_JOINING_GROUP_ROHINGYA_YEH,              
  UC_JOINING_GROUP_STRAIGHT_WAW,              
  UC_JOINING_GROUP_MANICHAEAN_ALEPH,          
  UC_JOINING_GROUP_MANICHAEAN_BETH,           
  UC_JOINING_GROUP_MANICHAEAN_GIMEL,          
  UC_JOINING_GROUP_MANICHAEAN_DALETH,         
  UC_JOINING_GROUP_MANICHAEAN_WAW,            
  UC_JOINING_GROUP_MANICHAEAN_ZAYIN,          
  UC_JOINING_GROUP_MANICHAEAN_HETH,           
  UC_JOINING_GROUP_MANICHAEAN_TETH,           
  UC_JOINING_GROUP_MANICHAEAN_YODH,           
  UC_JOINING_GROUP_MANICHAEAN_KAPH,           
  UC_JOINING_GROUP_MANICHAEAN_LAMEDH,         
  UC_JOINING_GROUP_MANICHAEAN_DHAMEDH,        
  UC_JOINING_GROUP_MANICHAEAN_THAMEDH,        
  UC_JOINING_GROUP_MANICHAEAN_MEM,            
  UC_JOINING_GROUP_MANICHAEAN_NUN,            
  UC_JOINING_GROUP_MANICHAEAN_SAMEKH,         
  UC_JOINING_GROUP_MANICHAEAN_AYIN,           
  UC_JOINING_GROUP_MANICHAEAN_PE,             
  UC_JOINING_GROUP_MANICHAEAN_SADHE,          
  UC_JOINING_GROUP_MANICHAEAN_QOPH,           
  UC_JOINING_GROUP_MANICHAEAN_RESH,           
  UC_JOINING_GROUP_MANICHAEAN_TAW,            
  UC_JOINING_GROUP_MANICHAEAN_ONE,            
  UC_JOINING_GROUP_MANICHAEAN_FIVE,           
  UC_JOINING_GROUP_MANICHAEAN_TEN,            
  UC_JOINING_GROUP_MANICHAEAN_TWENTY,         
  UC_JOINING_GROUP_MANICHAEAN_HUNDRED,        
  UC_JOINING_GROUP_AFRICAN_FEH,               
  UC_JOINING_GROUP_AFRICAN_QAF,               
  UC_JOINING_GROUP_AFRICAN_NOON,              
  UC_JOINING_GROUP_MALAYALAM_NGA,             
  UC_JOINING_GROUP_MALAYALAM_JA,              
  UC_JOINING_GROUP_MALAYALAM_NYA,             
  UC_JOINING_GROUP_MALAYALAM_TTA,             
  UC_JOINING_GROUP_MALAYALAM_NNA,             
  UC_JOINING_GROUP_MALAYALAM_NNNA,            
  UC_JOINING_GROUP_MALAYALAM_BHA,             
  UC_JOINING_GROUP_MALAYALAM_RA,              
  UC_JOINING_GROUP_MALAYALAM_LLA,             
  UC_JOINING_GROUP_MALAYALAM_LLLA,            
  UC_JOINING_GROUP_MALAYALAM_SSA,             
  UC_JOINING_GROUP_HANIFI_ROHINGYA_PA,        
  UC_JOINING_GROUP_HANIFI_ROHINGYA_KINNA_YA,  
  UC_JOINING_GROUP_THIN_YEH,                  
  UC_JOINING_GROUP_VERTICAL_TAIL              
};

 
extern const char *
       uc_joining_group_name (int joining_group)
       _UC_ATTRIBUTE_CONST;

 
extern int
       uc_joining_group_byname (const char *joining_group_name)
       _UC_ATTRIBUTE_PURE;

 
extern int
       uc_joining_group (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 

 

 
typedef struct
{
  bool (*test_fn) (ucs4_t uc);
}
uc_property_t;

 
 
extern @GNULIB_UNICTYPE_PROPERTY_WHITE_SPACE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_WHITE_SPACE;
extern @GNULIB_UNICTYPE_PROPERTY_ALPHABETIC_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_ALPHABETIC;
extern @GNULIB_UNICTYPE_PROPERTY_OTHER_ALPHABETIC_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_OTHER_ALPHABETIC;
extern @GNULIB_UNICTYPE_PROPERTY_NOT_A_CHARACTER_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_NOT_A_CHARACTER;
extern @GNULIB_UNICTYPE_PROPERTY_DEFAULT_IGNORABLE_CODE_POINT_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_DEFAULT_IGNORABLE_CODE_POINT;
extern @GNULIB_UNICTYPE_PROPERTY_OTHER_DEFAULT_IGNORABLE_CODE_POINT_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_OTHER_DEFAULT_IGNORABLE_CODE_POINT;
extern @GNULIB_UNICTYPE_PROPERTY_DEPRECATED_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_DEPRECATED;
extern @GNULIB_UNICTYPE_PROPERTY_LOGICAL_ORDER_EXCEPTION_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_LOGICAL_ORDER_EXCEPTION;
extern @GNULIB_UNICTYPE_PROPERTY_VARIATION_SELECTOR_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_VARIATION_SELECTOR;
extern @GNULIB_UNICTYPE_PROPERTY_PRIVATE_USE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_PRIVATE_USE;
extern @GNULIB_UNICTYPE_PROPERTY_UNASSIGNED_CODE_VALUE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_UNASSIGNED_CODE_VALUE;
 
extern @GNULIB_UNICTYPE_PROPERTY_UPPERCASE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_UPPERCASE;
extern @GNULIB_UNICTYPE_PROPERTY_OTHER_UPPERCASE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_OTHER_UPPERCASE;
extern @GNULIB_UNICTYPE_PROPERTY_LOWERCASE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_LOWERCASE;
extern @GNULIB_UNICTYPE_PROPERTY_OTHER_LOWERCASE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_OTHER_LOWERCASE;
extern @GNULIB_UNICTYPE_PROPERTY_TITLECASE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_TITLECASE;
extern @GNULIB_UNICTYPE_PROPERTY_CASED_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_CASED;
extern @GNULIB_UNICTYPE_PROPERTY_CASE_IGNORABLE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_CASE_IGNORABLE;
extern @GNULIB_UNICTYPE_PROPERTY_CHANGES_WHEN_LOWERCASED_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_CHANGES_WHEN_LOWERCASED;
extern @GNULIB_UNICTYPE_PROPERTY_CHANGES_WHEN_UPPERCASED_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_CHANGES_WHEN_UPPERCASED;
extern @GNULIB_UNICTYPE_PROPERTY_CHANGES_WHEN_TITLECASED_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_CHANGES_WHEN_TITLECASED;
extern @GNULIB_UNICTYPE_PROPERTY_CHANGES_WHEN_CASEFOLDED_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_CHANGES_WHEN_CASEFOLDED;
extern @GNULIB_UNICTYPE_PROPERTY_CHANGES_WHEN_CASEMAPPED_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_CHANGES_WHEN_CASEMAPPED;
extern @GNULIB_UNICTYPE_PROPERTY_SOFT_DOTTED_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_SOFT_DOTTED;
 
extern @GNULIB_UNICTYPE_PROPERTY_ID_START_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_ID_START;
extern @GNULIB_UNICTYPE_PROPERTY_OTHER_ID_START_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_OTHER_ID_START;
extern @GNULIB_UNICTYPE_PROPERTY_ID_CONTINUE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_ID_CONTINUE;
extern @GNULIB_UNICTYPE_PROPERTY_OTHER_ID_CONTINUE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_OTHER_ID_CONTINUE;
extern @GNULIB_UNICTYPE_PROPERTY_XID_START_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_XID_START;
extern @GNULIB_UNICTYPE_PROPERTY_XID_CONTINUE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_XID_CONTINUE;
extern @GNULIB_UNICTYPE_PROPERTY_PATTERN_WHITE_SPACE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_PATTERN_WHITE_SPACE;
extern @GNULIB_UNICTYPE_PROPERTY_PATTERN_SYNTAX_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_PATTERN_SYNTAX;
 
extern @GNULIB_UNICTYPE_PROPERTY_JOIN_CONTROL_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_JOIN_CONTROL;
extern @GNULIB_UNICTYPE_PROPERTY_GRAPHEME_BASE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_GRAPHEME_BASE;
extern @GNULIB_UNICTYPE_PROPERTY_GRAPHEME_EXTEND_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_GRAPHEME_EXTEND;
extern @GNULIB_UNICTYPE_PROPERTY_OTHER_GRAPHEME_EXTEND_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_OTHER_GRAPHEME_EXTEND;
extern @GNULIB_UNICTYPE_PROPERTY_GRAPHEME_LINK_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_GRAPHEME_LINK;
 
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_CONTROL_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_CONTROL;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_LEFT_TO_RIGHT_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_LEFT_TO_RIGHT;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_HEBREW_RIGHT_TO_LEFT_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_HEBREW_RIGHT_TO_LEFT;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_ARABIC_RIGHT_TO_LEFT_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_ARABIC_RIGHT_TO_LEFT;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_EUROPEAN_DIGIT_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_EUROPEAN_DIGIT;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_EUR_NUM_SEPARATOR_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_EUR_NUM_SEPARATOR;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_EUR_NUM_TERMINATOR_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_EUR_NUM_TERMINATOR;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_ARABIC_DIGIT_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_ARABIC_DIGIT;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_COMMON_SEPARATOR_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_COMMON_SEPARATOR;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_BLOCK_SEPARATOR_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_BLOCK_SEPARATOR;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_SEGMENT_SEPARATOR_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_SEGMENT_SEPARATOR;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_WHITESPACE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_WHITESPACE;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_NON_SPACING_MARK_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_NON_SPACING_MARK;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_BOUNDARY_NEUTRAL_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_BOUNDARY_NEUTRAL;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_PDF_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_PDF;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_EMBEDDING_OR_OVERRIDE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_EMBEDDING_OR_OVERRIDE;
extern @GNULIB_UNICTYPE_PROPERTY_BIDI_OTHER_NEUTRAL_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_BIDI_OTHER_NEUTRAL;
 
extern @GNULIB_UNICTYPE_PROPERTY_HEX_DIGIT_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_HEX_DIGIT;
extern @GNULIB_UNICTYPE_PROPERTY_ASCII_HEX_DIGIT_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_ASCII_HEX_DIGIT;
 
extern @GNULIB_UNICTYPE_PROPERTY_IDEOGRAPHIC_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_IDEOGRAPHIC;
extern @GNULIB_UNICTYPE_PROPERTY_UNIFIED_IDEOGRAPH_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_UNIFIED_IDEOGRAPH;
extern @GNULIB_UNICTYPE_PROPERTY_RADICAL_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_RADICAL;
extern @GNULIB_UNICTYPE_PROPERTY_IDS_BINARY_OPERATOR_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_IDS_BINARY_OPERATOR;
extern @GNULIB_UNICTYPE_PROPERTY_IDS_TRINARY_OPERATOR_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_IDS_TRINARY_OPERATOR;
 
extern @GNULIB_UNICTYPE_PROPERTY_EMOJI_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_EMOJI;
extern @GNULIB_UNICTYPE_PROPERTY_EMOJI_PRESENTATION_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_EMOJI_PRESENTATION;
extern @GNULIB_UNICTYPE_PROPERTY_EMOJI_MODIFIER_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_EMOJI_MODIFIER;
extern @GNULIB_UNICTYPE_PROPERTY_EMOJI_MODIFIER_BASE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_EMOJI_MODIFIER_BASE;
extern @GNULIB_UNICTYPE_PROPERTY_EMOJI_COMPONENT_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_EMOJI_COMPONENT;
extern @GNULIB_UNICTYPE_PROPERTY_EXTENDED_PICTOGRAPHIC_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_EXTENDED_PICTOGRAPHIC;
 
extern @GNULIB_UNICTYPE_PROPERTY_ZERO_WIDTH_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_ZERO_WIDTH;
extern @GNULIB_UNICTYPE_PROPERTY_SPACE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_SPACE;
extern @GNULIB_UNICTYPE_PROPERTY_NON_BREAK_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_NON_BREAK;
extern @GNULIB_UNICTYPE_PROPERTY_ISO_CONTROL_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_ISO_CONTROL;
extern @GNULIB_UNICTYPE_PROPERTY_FORMAT_CONTROL_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_FORMAT_CONTROL;
extern @GNULIB_UNICTYPE_PROPERTY_DASH_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_DASH;
extern @GNULIB_UNICTYPE_PROPERTY_HYPHEN_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_HYPHEN;
extern @GNULIB_UNICTYPE_PROPERTY_PUNCTUATION_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_PUNCTUATION;
extern @GNULIB_UNICTYPE_PROPERTY_LINE_SEPARATOR_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_LINE_SEPARATOR;
extern @GNULIB_UNICTYPE_PROPERTY_PARAGRAPH_SEPARATOR_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_PARAGRAPH_SEPARATOR;
extern @GNULIB_UNICTYPE_PROPERTY_QUOTATION_MARK_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_QUOTATION_MARK;
extern @GNULIB_UNICTYPE_PROPERTY_SENTENCE_TERMINAL_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_SENTENCE_TERMINAL;
extern @GNULIB_UNICTYPE_PROPERTY_TERMINAL_PUNCTUATION_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_TERMINAL_PUNCTUATION;
extern @GNULIB_UNICTYPE_PROPERTY_CURRENCY_SYMBOL_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_CURRENCY_SYMBOL;
extern @GNULIB_UNICTYPE_PROPERTY_MATH_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_MATH;
extern @GNULIB_UNICTYPE_PROPERTY_OTHER_MATH_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_OTHER_MATH;
extern @GNULIB_UNICTYPE_PROPERTY_PAIRED_PUNCTUATION_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_PAIRED_PUNCTUATION;
extern @GNULIB_UNICTYPE_PROPERTY_LEFT_OF_PAIR_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_LEFT_OF_PAIR;
extern @GNULIB_UNICTYPE_PROPERTY_COMBINING_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_COMBINING;
extern @GNULIB_UNICTYPE_PROPERTY_COMPOSITE_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_COMPOSITE;
extern @GNULIB_UNICTYPE_PROPERTY_DECIMAL_DIGIT_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_DECIMAL_DIGIT;
extern @GNULIB_UNICTYPE_PROPERTY_NUMERIC_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_NUMERIC;
extern @GNULIB_UNICTYPE_PROPERTY_DIACRITIC_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_DIACRITIC;
extern @GNULIB_UNICTYPE_PROPERTY_EXTENDER_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_EXTENDER;
extern @GNULIB_UNICTYPE_PROPERTY_IGNORABLE_CONTROL_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_IGNORABLE_CONTROL;
extern @GNULIB_UNICTYPE_PROPERTY_REGIONAL_INDICATOR_DLL_VARIABLE@ const uc_property_t UC_PROPERTY_REGIONAL_INDICATOR;

 
extern uc_property_t
       uc_property_byname (const char *property_name);

 
#define uc_property_is_valid(property) ((property).test_fn != NULL)

 
extern bool
       uc_is_property (ucs4_t uc, uc_property_t property);
extern bool uc_is_property_white_space (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_alphabetic (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_other_alphabetic (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_not_a_character (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_default_ignorable_code_point (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_other_default_ignorable_code_point (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_deprecated (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_logical_order_exception (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_variation_selector (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_private_use (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_unassigned_code_value (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_uppercase (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_other_uppercase (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_lowercase (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_other_lowercase (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_titlecase (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_cased (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_case_ignorable (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_changes_when_lowercased (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_changes_when_uppercased (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_changes_when_titlecased (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_changes_when_casefolded (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_changes_when_casemapped (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_soft_dotted (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_id_start (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_other_id_start (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_id_continue (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_other_id_continue (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_xid_start (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_xid_continue (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_pattern_white_space (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_pattern_syntax (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_join_control (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_grapheme_base (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_grapheme_extend (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_other_grapheme_extend (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_grapheme_link (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_control (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_left_to_right (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_hebrew_right_to_left (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_arabic_right_to_left (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_european_digit (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_eur_num_separator (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_eur_num_terminator (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_arabic_digit (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_common_separator (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_block_separator (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_segment_separator (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_whitespace (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_non_spacing_mark (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_boundary_neutral (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_pdf (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_embedding_or_override (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_bidi_other_neutral (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_hex_digit (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_ascii_hex_digit (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_ideographic (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_unified_ideograph (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_radical (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_ids_binary_operator (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_ids_trinary_operator (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_emoji (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_emoji_presentation (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_emoji_modifier (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_emoji_modifier_base (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_emoji_component (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_extended_pictographic (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_zero_width (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_space (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_non_break (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_iso_control (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_format_control (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_dash (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_hyphen (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_punctuation (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_line_separator (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_paragraph_separator (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_quotation_mark (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_sentence_terminal (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_terminal_punctuation (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_currency_symbol (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_math (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_other_math (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_paired_punctuation (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_left_of_pair (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_combining (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_composite (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_decimal_digit (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_numeric (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_diacritic (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_extender (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_ignorable_control (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
extern bool uc_is_property_regional_indicator (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 

 

typedef struct
{
  unsigned int code : 21;
  unsigned int start : 1;
  unsigned int end : 1;
}
uc_interval_t;
typedef struct
{
  unsigned int nintervals;
  const uc_interval_t *intervals;
  const char *name;
}
uc_script_t;

 
extern const uc_script_t *
       uc_script (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern const uc_script_t *
       uc_script_byname (const char *script_name)
       _UC_ATTRIBUTE_PURE;

 
extern bool
       uc_is_script (ucs4_t uc, const uc_script_t *script)
       _UC_ATTRIBUTE_PURE;

 
extern void
       uc_all_scripts (const uc_script_t **scripts, size_t *count);

 

 

typedef struct
{
  ucs4_t start;
  ucs4_t end;
  const char *name;
}
uc_block_t;

 
extern const uc_block_t *
       uc_block (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_block (ucs4_t uc, const uc_block_t *block)
       _UC_ATTRIBUTE_PURE;

 
extern void
       uc_all_blocks (const uc_block_t **blocks, size_t *count);

 

 

 
extern bool
       uc_is_c_whitespace (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_java_whitespace (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

enum
{
  UC_IDENTIFIER_START,     
  UC_IDENTIFIER_VALID,     
  UC_IDENTIFIER_INVALID,   
  UC_IDENTIFIER_IGNORABLE  
};

 
extern int
       uc_c_ident_category (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern int
       uc_java_ident_category (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 

 

 
extern bool
       uc_is_alnum (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_alpha (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_cntrl (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_digit (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_graph (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_lower (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_print (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_punct (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_space (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_upper (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
extern bool
       uc_is_xdigit (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 
 
extern bool
       uc_is_blank (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

 

#ifdef __cplusplus
}
#endif

#endif  
