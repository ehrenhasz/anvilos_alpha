





#if 0
# define SPELL_PRINTTREE
#endif



#if 0
# define SPELL_COMPRESS_ALWAYS
#endif



#if 0
# define DEBUG_TRIEWALK
#endif

#define MAXWLEN 254		
				
				
				

#define MAXREGIONS 8		



typedef int idx_T;

typedef int salfirst_T;


typedef struct slang_S slang_T;
struct slang_S
{
    slang_T	*sl_next;	
    char_u	*sl_name;	
    char_u	*sl_fname;	
    int		sl_add;		

    char_u	*sl_fbyts;	
    long	sl_fbyts_len;	
    idx_T	*sl_fidxs;	
    char_u	*sl_kbyts;	
    idx_T	*sl_kidxs;	
    char_u	*sl_pbyts;	
    idx_T	*sl_pidxs;	

    char_u	*sl_info;	

    char_u	sl_regions[MAXREGIONS * 2 + 1];
				

    char_u	*sl_midword;	

    hashtab_T	sl_wordcount;	

    int		sl_compmax;	
    int		sl_compminlen;	
    int		sl_compsylmax;	
    int		sl_compoptions;	
    garray_T	sl_comppat;	
    regprog_T	*sl_compprog;	
				
    char_u	*sl_comprules;	
    char_u	*sl_compstartflags; 
    char_u	*sl_compallflags; 
    char_u	sl_nobreak;	
    char_u	*sl_syllable;	
    garray_T	sl_syl_items;	

    int		sl_prefixcnt;	
    regprog_T	**sl_prefprog;	

    garray_T	sl_rep;		
    short	sl_rep_first[256];  
				    
    garray_T	sl_sal;		
    salfirst_T	sl_sal_first[256];  
				    
    int		sl_followup;	
    int		sl_collapse;	
    int		sl_rem_accents;	
    int		sl_sofo;	
				
				
    garray_T	sl_repsal;	
    short	sl_repsal_first[256];  
    int		sl_nosplitsugs;	
    int		sl_nocompoundsugs; 

    
    time_t	sl_sugtime;	
    char_u	*sl_sbyts;	
    idx_T	*sl_sidxs;	
    buf_T	*sl_sugbuf;	
    int		sl_sugloaded;	
				

    int		sl_has_map;	
    hashtab_T	sl_map_hash;	
    int		sl_map_array[256]; 
    hashtab_T	sl_sounddone;	
				
};

#ifdef VMS
# define SPL_FNAME_TMPL  "%s_%s.spl"
# define SPL_FNAME_ADD   "_add."
# define SPL_FNAME_ASCII "_ascii."
#else
# define SPL_FNAME_TMPL  "%s.%s.spl"
# define SPL_FNAME_ADD   ".add."
# define SPL_FNAME_ASCII ".ascii."
#endif



#define WF_REGION   0x01	
#define WF_ONECAP   0x02	
#define WF_ALLCAP   0x04	
#define WF_RARE	    0x08	
#define WF_BANNED   0x10	
#define WF_AFX	    0x20	
#define WF_FIXCAP   0x40	
#define WF_KEEPCAP  0x80	

#define WF_CAPMASK (WF_ONECAP | WF_ALLCAP | WF_KEEPCAP | WF_FIXCAP)


#define WF_HAS_AFF  0x0100	
#define WF_NEEDCOMP 0x0200	
#define WF_NOSUGGEST 0x0400	
#define WF_COMPROOT 0x0800	
#define WF_NOCOMPBEF 0x1000	
#define WF_NOCOMPAFT 0x2000	


#define WFP_RARE	    0x01	
#define WFP_NC		    0x02	
#define WFP_UP		    0x04	
#define WFP_COMPPERMIT	    0x08	
#define WFP_COMPFORBID	    0x10	



#define WF_RAREPFX  (WFP_RARE << 24)	
#define WF_PFX_NC   (WFP_NC << 24)	
#define WF_PFX_UP   (WFP_UP << 24)	
#define WF_PFX_COMPPERMIT (WFP_COMPPERMIT << 24) 
						 
#define WF_PFX_COMPFORBID (WFP_COMPFORBID << 24) 
						 


#define COMP_CHECKDUP		1	
#define COMP_CHECKREP		2	
#define COMP_CHECKCASE		4	
#define COMP_CHECKTRIPLE	8	




typedef struct fromto_S
{
    char_u	*ft_from;
    char_u	*ft_to;
} fromto_T;




typedef struct salitem_S
{
    char_u	*sm_lead;	
    int		sm_leadlen;	
    char_u	*sm_oneof;	
    char_u	*sm_rules;	
    char_u	*sm_to;		
    int		*sm_lead_w;	
    int		*sm_oneof_w;	
    int		*sm_to_w;	
} salitem_T;



#define	SP_TRUNCERROR	(-1)	
#define	SP_FORMERROR	(-2)	
#define SP_OTHERERROR	(-3)	


typedef struct langp_S
{
    slang_T	*lp_slang;	
    slang_T	*lp_sallang;	
    slang_T	*lp_replang;	
    int		lp_region;	
} langp_T;

#define LANGP_ENTRY(ga, i)	(((langp_T *)(ga).ga_data) + (i))

#define VIMSUGMAGIC "VIMsug"	
#define VIMSUGMAGICL 6
#define VIMSUGVERSION 1


typedef struct spelltab_S
{
    char_u  st_isw[256];	
    char_u  st_isu[256];	
    char_u  st_fold[256];	
    char_u  st_upper[256];	
} spelltab_T;


#if defined(HAVE_WCHAR_H)
# include <wchar.h>	    
#endif



#ifdef HAVE_TOWLOWER
# define SPELL_TOFOLD(c) (enc_utf8 && (c) >= 128 ? utf_fold(c) \
	    : (c) < 256 ? (int)spelltab.st_fold[c] : (int)towlower(c))
#else
# define SPELL_TOFOLD(c) (enc_utf8 && (c) >= 128 ? utf_fold(c) \
	    : (c) < 256 ? (int)spelltab.st_fold[c] : (c))
#endif

#ifdef HAVE_TOWUPPER
# define SPELL_TOUPPER(c) (enc_utf8 && (c) >= 128 ? utf_toupper(c) \
	    : (c) < 256 ? (int)spelltab.st_upper[c] : (int)towupper(c))
#else
# define SPELL_TOUPPER(c) (enc_utf8 && (c) >= 128 ? utf_toupper(c) \
	    : (c) < 256 ? (int)spelltab.st_upper[c] : (c))
#endif

#ifdef HAVE_ISWUPPER
# define SPELL_ISUPPER(c) (enc_utf8 && (c) >= 128 ? utf_isupper(c) \
	    : (c) < 256 ? spelltab.st_isu[c] : iswupper(c))
#else
# define SPELL_ISUPPER(c) (enc_utf8 && (c) >= 128 ? utf_isupper(c) \
	    : (c) < 256 ? spelltab.st_isu[c] : (FALSE))
#endif

#ifdef FEAT_SPELL
# ifdef IN_SPELL_C
#  define SPELL_EXTERN
#  define SPELL_INIT(x) x
# else
#  define SPELL_EXTERN extern
#  define SPELL_INIT(x)
# endif



SPELL_EXTERN slang_T	*first_lang SPELL_INIT(= NULL);


SPELL_EXTERN char_u	*int_wordlist SPELL_INIT(= NULL);

SPELL_EXTERN spelltab_T   spelltab;
SPELL_EXTERN int	  did_set_spelltab;


#define SPELL_ADD_GOOD	0
#define SPELL_ADD_BAD	1
#define SPELL_ADD_RARE	2

typedef struct wordcount_S
{
    short_u	wc_count;	    
    char_u	wc_word[1];	    
} wordcount_T;

#define WC_KEY_OFF  offsetof(wordcount_T, wc_word)
#define HI2WC(hi)     ((wordcount_T *)((hi)->hi_key - WC_KEY_OFF))
#define MAXWORDCOUNT 0xffff


SPELL_EXTERN char_u	*repl_from SPELL_INIT(= NULL);
SPELL_EXTERN char_u	*repl_to SPELL_INIT(= NULL);
#endif
