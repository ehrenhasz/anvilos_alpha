




#if defined(SASC) && SASC < 658
typedef long		linenr_T;
typedef int		colnr_T;
typedef unsigned short	short_u;
#endif


typedef struct
{
    linenr_T	lnum;	
    colnr_T	col;	
    colnr_T	coladd; 
} pos_T;



typedef struct
{
    linenr_T	lnum;	
    colnr_T	col;	
} lpos_T;


typedef struct growarray
{
    int	    ga_len;		    
    int	    ga_maxlen;		    
    int	    ga_itemsize;	    
    int	    ga_growsize;	    
    void    *ga_data;		    
} garray_T;

#define GA_EMPTY    {0, 0, 0, 0, NULL}

typedef struct window_S		win_T;
typedef struct wininfo_S	wininfo_T;
typedef struct frame_S		frame_T;
typedef int			scid_T;		
typedef struct file_buffer	buf_T;		
typedef struct terminal_S	term_T;

#ifdef FEAT_MENU
typedef struct VimMenu vimmenu_T;
#endif


#define SCRIPT_VERSION_MAX 4

#define SCRIPT_VERSION_VIM9 999999


typedef struct {
#ifdef FEAT_EVAL
    scid_T	sc_sid;		
    int		sc_seq;		
    linenr_T	sc_lnum;	
#endif
    int		sc_version;	
} sctx_T;


typedef struct {
    buf_T   *br_buf;
    int	    br_fnum;
    int	    br_buf_free_count;
} bufref_T;


#include "regexp.h"


#ifdef FEAT_GUI
# include "gui.h"
#else
# ifdef FEAT_XCLIPBOARD
#  include <X11/Intrinsic.h>
# endif
# define guicolor_T long
# define INVALCOLOR ((guicolor_T)0x1ffffff)
    
# define CTERMCOLOR ((guicolor_T)0x1fffffe)
#endif
#define COLOR_INVALID(x) ((x) == INVALCOLOR || (x) == CTERMCOLOR)

#ifdef FEAT_TERMINAL
# include "libvterm/include/vterm.h"
typedef struct {
    VTermColor	fg;
    VTermColor	bg;
} termcellcolor_T;
#endif



#define NMARKS		('z' - 'a' + 1)	
#define EXTRA_MARKS	10		
#define JUMPLISTSIZE	100		
#define TAGSTACKSIZE	20		

typedef struct filemark
{
    pos_T	mark;		
    int		fnum;		
} fmark_T;


typedef struct xfilemark
{
    fmark_T	fmark;
    char_u	*fname;		
#ifdef FEAT_VIMINFO
    time_T	time_set;
#endif
} xfmark_T;


typedef struct taggy
{
    char_u	*tagname;	
    fmark_T	fmark;		
    int		cur_match;	
    int		cur_fnum;	
    char_u	*user_data;	
} taggy_T;


typedef struct
{
#ifdef FEAT_ARABIC
    int		wo_arab;
# define w_p_arab w_onebuf_opt.wo_arab	
#endif
#ifdef FEAT_LINEBREAK
    int		wo_bri;
# define w_p_bri w_onebuf_opt.wo_bri	
    char_u	*wo_briopt;
# define w_p_briopt w_onebuf_opt.wo_briopt 
#endif
    char_u	*wo_wcr;
# define w_p_wcr w_onebuf_opt.wo_wcr	
#ifdef FEAT_DIFF
    int		wo_diff;
# define w_p_diff w_onebuf_opt.wo_diff	
#endif
#ifdef FEAT_FOLDING
    long	wo_fdc;
# define w_p_fdc w_onebuf_opt.wo_fdc	
    int		wo_fdc_save;
# define w_p_fdc_save w_onebuf_opt.wo_fdc_save	
    int		wo_fen;
# define w_p_fen w_onebuf_opt.wo_fen	
    int		wo_fen_save;
# define w_p_fen_save w_onebuf_opt.wo_fen_save	
    char_u	*wo_fdi;
# define w_p_fdi w_onebuf_opt.wo_fdi	
    long	wo_fdl;
# define w_p_fdl w_onebuf_opt.wo_fdl	
    int		wo_fdl_save;
# define w_p_fdl_save w_onebuf_opt.wo_fdl_save	
    char_u	*wo_fdm;
# define w_p_fdm w_onebuf_opt.wo_fdm	
    char_u	*wo_fdm_save;
# define w_p_fdm_save w_onebuf_opt.wo_fdm_save	
    long	wo_fml;
# define w_p_fml w_onebuf_opt.wo_fml	
    long	wo_fdn;
# define w_p_fdn w_onebuf_opt.wo_fdn	
# ifdef FEAT_EVAL
    char_u	*wo_fde;
# define w_p_fde w_onebuf_opt.wo_fde	
    char_u	*wo_fdt;
#  define w_p_fdt w_onebuf_opt.wo_fdt	
# endif
    char_u	*wo_fmr;
# define w_p_fmr w_onebuf_opt.wo_fmr	
#endif
#ifdef FEAT_LINEBREAK
    int		wo_lbr;
# define w_p_lbr w_onebuf_opt.wo_lbr	
#endif
    int		wo_list;
#define w_p_list w_onebuf_opt.wo_list	
    char_u	*wo_lcs;
#define w_p_lcs w_onebuf_opt.wo_lcs	
    char_u	*wo_fcs;
#define w_p_fcs w_onebuf_opt.wo_fcs	
    int		wo_nu;
#define w_p_nu w_onebuf_opt.wo_nu	
    int		wo_rnu;
#define w_p_rnu w_onebuf_opt.wo_rnu	
    char_u	*wo_ve;
#define w_p_ve w_onebuf_opt.wo_ve	
    unsigned	wo_ve_flags;
#define	w_ve_flags w_onebuf_opt.wo_ve_flags	
#ifdef FEAT_LINEBREAK
    long	wo_nuw;
# define w_p_nuw w_onebuf_opt.wo_nuw	
#endif
    int		wo_wfh;
# define w_p_wfh w_onebuf_opt.wo_wfh	
    int		wo_wfw;
# define w_p_wfw w_onebuf_opt.wo_wfw	
#if defined(FEAT_QUICKFIX)
    int		wo_pvw;
# define w_p_pvw w_onebuf_opt.wo_pvw	
#endif
#ifdef FEAT_RIGHTLEFT
    int		wo_rl;
# define w_p_rl w_onebuf_opt.wo_rl	
    char_u	*wo_rlc;
# define w_p_rlc w_onebuf_opt.wo_rlc	
#endif
    long	wo_scr;
#define w_p_scr w_onebuf_opt.wo_scr	
    int		wo_sms;
#define w_p_sms w_onebuf_opt.wo_sms	
#ifdef FEAT_SPELL
    int		wo_spell;
# define w_p_spell w_onebuf_opt.wo_spell 
#endif
#if defined(FEAT_SYN_HL) || defined(FEAT_FOLDING) || defined(FEAT_DIFF)
    int		wo_cuc;
# define w_p_cuc w_onebuf_opt.wo_cuc	
    int		wo_cul;
# define w_p_cul w_onebuf_opt.wo_cul	
    char_u	*wo_culopt;
# define w_p_culopt w_onebuf_opt.wo_culopt	
    char_u	*wo_cc;
# define w_p_cc w_onebuf_opt.wo_cc	
#endif
#ifdef FEAT_LINEBREAK
    char_u	*wo_sbr;
#define w_p_sbr w_onebuf_opt.wo_sbr	
#endif
#ifdef FEAT_STL_OPT
    char_u	*wo_stl;
#define w_p_stl w_onebuf_opt.wo_stl	
#endif
    int		wo_scb;
#define w_p_scb w_onebuf_opt.wo_scb	
    int		wo_diff_saved; 
#define w_p_diff_saved w_onebuf_opt.wo_diff_saved
    int		wo_scb_save;	
#define w_p_scb_save w_onebuf_opt.wo_scb_save
    int		wo_wrap;
#define w_p_wrap w_onebuf_opt.wo_wrap	
#ifdef FEAT_DIFF
    int		wo_wrap_save;	
# define w_p_wrap_save w_onebuf_opt.wo_wrap_save
#endif
#ifdef FEAT_CONCEAL
    char_u	*wo_cocu;		
# define w_p_cocu w_onebuf_opt.wo_cocu
    long	wo_cole;		
# define w_p_cole w_onebuf_opt.wo_cole
#endif
    int		wo_crb;
#define w_p_crb w_onebuf_opt.wo_crb	
    int		wo_crb_save;	
#define w_p_crb_save w_onebuf_opt.wo_crb_save
#ifdef FEAT_SIGNS
    char_u	*wo_scl;
# define w_p_scl w_onebuf_opt.wo_scl	
#endif
    long	wo_siso;
# define w_p_siso w_onebuf_opt.wo_siso	
    long	wo_so;
# define w_p_so w_onebuf_opt.wo_so	
#ifdef FEAT_TERMINAL
    char_u	*wo_twk;
# define w_p_twk w_onebuf_opt.wo_twk	
    char_u	*wo_tws;
# define w_p_tws w_onebuf_opt.wo_tws	
#endif

#ifdef FEAT_EVAL
    sctx_T	wo_script_ctx[WV_COUNT];	
# define w_p_script_ctx w_onebuf_opt.wo_script_ctx
#endif
} winopt_T;


struct wininfo_S
{
    wininfo_T	*wi_next;	
    wininfo_T	*wi_prev;	
    win_T	*wi_win;	
    pos_T	wi_fpos;	
    winopt_T	wi_opt;		
    int		wi_optset;	
#ifdef FEAT_FOLDING
    int		wi_fold_manual;	
    garray_T	wi_folds;	
#endif
    int		wi_changelistidx; 
};


typedef struct foldinfo
{
    int		fi_level;	
				
    int		fi_lnum;	
    int		fi_low_level;	
				
} foldinfo_T;


typedef struct
{
    pos_T	vi_start;	
    pos_T	vi_end;		
    int		vi_mode;	
    colnr_T	vi_curswant;	
} visualinfo_T;





typedef struct {
    char_u	*ul_line;	
    long	ul_len;		
				
} undoline_T;

typedef struct u_entry u_entry_T;
typedef struct u_header u_header_T;
struct u_entry
{
    u_entry_T	*ue_next;	
    linenr_T	ue_top;		
    linenr_T	ue_bot;		
    linenr_T	ue_lcount;	
    undoline_T	*ue_array;	
    long	ue_size;	
#ifdef U_DEBUG
    int		ue_magic;	
#endif
};

struct u_header
{
    
    
    union {
	u_header_T *ptr;	
	long	   seq;
    } uh_next;
    union {
	u_header_T *ptr;	
	long	   seq;
    } uh_prev;
    union {
	u_header_T *ptr;	
	long	   seq;
    } uh_alt_next;
    union {
	u_header_T *ptr;	
	long	   seq;
    } uh_alt_prev;
    long	uh_seq;		
    int		uh_walk;	
    u_entry_T	*uh_entry;	
    u_entry_T	*uh_getbot_entry; 
    pos_T	uh_cursor;	
    long	uh_cursor_vcol;
    int		uh_flags;	
    pos_T	uh_namedm[NMARKS];	
    visualinfo_T uh_visual;	
    time_T	uh_time;	
    long	uh_save_nr;	
				
#ifdef U_DEBUG
    int		uh_magic;	
#endif
};


#define UH_CHANGED  0x01	
#define UH_EMPTYBUF 0x02	


#define ALIGN_LONG	
#define ALIGN_SIZE (sizeof(long))

#define ALIGN_MASK (ALIGN_SIZE - 1)

typedef struct m_info minfo_T;


struct m_info
{
#ifdef ALIGN_LONG
    long_u	m_size;		
#else
    short_u	m_size;		
#endif
    minfo_T	*m_next;	
};



typedef struct block_hdr    bhdr_T;
typedef struct memfile	    memfile_T;
typedef long		    blocknr_T;



typedef struct mf_hashitem_S mf_hashitem_T;

struct mf_hashitem_S
{
    mf_hashitem_T   *mhi_next;
    mf_hashitem_T   *mhi_prev;
    blocknr_T	    mhi_key;
};

#define MHT_INIT_SIZE   64

typedef struct mf_hashtab_S
{
    long_u	    mht_mask;	    
				    
    long_u	    mht_count;	    
    mf_hashitem_T   **mht_buckets;  
				    
    mf_hashitem_T   *mht_small_buckets[MHT_INIT_SIZE];   
    char	    mht_fixed;	    
} mf_hashtab_T;



struct block_hdr
{
    mf_hashitem_T bh_hashitem;      
#define bh_bnum bh_hashitem.mhi_key 

    bhdr_T	*bh_next;	    
    bhdr_T	*bh_prev;	    
    char_u	*bh_data;	    
    int		bh_page_count;	    

#define BH_DIRTY    1
#define BH_LOCKED   2
    char	bh_flags;	    
};


typedef struct nr_trans NR_TRANS;

struct nr_trans
{
    mf_hashitem_T nt_hashitem;		
#define nt_old_bnum nt_hashitem.mhi_key	

    blocknr_T	nt_new_bnum;		
};


typedef struct buffblock buffblock_T;
typedef struct buffheader buffheader_T;


struct buffblock
{
    buffblock_T	*b_next;	
    char_u	b_str[1];	
};


struct buffheader
{
    buffblock_T	bh_first;	
    buffblock_T	*bh_curr;	
    int		bh_index;	
    int		bh_space;	
};

typedef struct
{
    buffheader_T sr_redobuff;
    buffheader_T sr_old_redobuff;
} save_redo_T;

typedef enum {
    XP_PREFIX_NONE,	
    XP_PREFIX_NO,	
    XP_PREFIX_INV,	
} xp_prefix_T;


typedef enum {
    OP_NONE = 0,
    OP_ADDING,		
    OP_PREPENDING,	
    OP_REMOVING,	
} set_op_T;


typedef struct expand
{
    char_u	*xp_pattern;		
					
    int		xp_context;		
    int		xp_pattern_len;		
    xp_prefix_T	xp_prefix;
#if defined(FEAT_EVAL)
    char_u	*xp_arg;		
    sctx_T	xp_script_ctx;		
#endif
    int		xp_backslash;		
#ifndef BACKSLASH_IN_FILENAME
    int		xp_shell;		
					
#endif
    int		xp_numfiles;		
					
    int		xp_col;			
    int		xp_selected;		
    char_u	*xp_orig;		
    char_u	**xp_files;		
    char_u	*xp_line;		
#define EXPAND_BUF_LEN 256
    char_u	xp_buf[EXPAND_BUF_LEN];	
} expand_T;


#define XP_BS_NONE	0	
#define XP_BS_ONE	0x1	
#define XP_BS_THREE	0x2	
#define XP_BS_COMMA	0x4	


typedef struct
{
    char_u	*cmdbuff;	
    int		cmdbufflen;	
    int		cmdlen;		
    int		cmdpos;		
    int		cmdspos;	
    int		cmdfirstc;	
    int		cmdindent;	
    char_u	*cmdprompt;	
    int		cmdattr;	
    int		overstrike;	
				
    expand_T	*xpc;		
				
    int		xp_context;	
# ifdef FEAT_EVAL
    char_u	*xp_arg;	
    int		input_fn;	
# endif
} cmdline_info_T;


typedef struct
{
    int		cmod_flags;		
#define CMOD_SANDBOX	    0x0001	
#define CMOD_SILENT	    0x0002	
#define CMOD_ERRSILENT	    0x0004	
#define CMOD_UNSILENT	    0x0008	
#define CMOD_NOAUTOCMD	    0x0010	
#define CMOD_HIDE	    0x0020	
#define CMOD_BROWSE	    0x0040	
#define CMOD_CONFIRM	    0x0080	
#define CMOD_KEEPALT	    0x0100	
#define CMOD_KEEPMARKS	    0x0200	
#define CMOD_KEEPJUMPS	    0x0400	
#define CMOD_LOCKMARKS	    0x0800	
#define CMOD_KEEPPATTERNS   0x1000	
#define CMOD_NOSWAPFILE	    0x2000	
#define CMOD_VIM9CMD	    0x4000	
#define CMOD_LEGACY	    0x8000	

    int		cmod_split;		
    int		cmod_tab;		
    regmatch_T	cmod_filter_regmatch;	
    int		cmod_filter_force;	

    int		cmod_verbose;		
					

    
    char_u	*cmod_save_ei;		
#ifdef HAVE_SANDBOX
    int		cmod_did_sandbox;	
#endif
    long	cmod_verbose_save;	
					
    int		cmod_save_msg_silent;	
					
    int		cmod_save_msg_scroll;	
    int		cmod_did_esilent;	
} cmdmod_T;

typedef enum {
    MF_DIRTY_NO = 0,		
    MF_DIRTY_YES,		
    MF_DIRTY_YES_NOSYNC,	
} mfdirty_T;

#define MF_SEED_LEN	8

struct memfile
{
    char_u	*mf_fname;		
    char_u	*mf_ffname;		
    int		mf_fd;			
    int		mf_flags;		
    int		mf_reopen;		
    bhdr_T	*mf_free_first;		
    bhdr_T	*mf_used_first;		
    bhdr_T	*mf_used_last;		
    unsigned	mf_used_count;		
    unsigned	mf_used_count_max;	
    mf_hashtab_T mf_hash;		
    mf_hashtab_T mf_trans;		
    blocknr_T	mf_blocknr_max;		
    blocknr_T	mf_blocknr_min;		
    blocknr_T	mf_neg_count;		
    blocknr_T	mf_infile_count;	
    unsigned	mf_page_size;		
    mfdirty_T	mf_dirty;
#ifdef FEAT_CRYPT
    buf_T	*mf_buffer;		
    char_u	mf_seed[MF_SEED_LEN];	

    
    
    char_u	*mf_old_key;
    int		mf_old_cm;
    char_u	mf_old_seed[MF_SEED_LEN];
#endif
};



typedef struct info_pointer
{
    blocknr_T	ip_bnum;	
    linenr_T	ip_low;		
    linenr_T	ip_high;	
    int		ip_index;	
} infoptr_T;	

#ifdef FEAT_BYTEOFF
typedef struct ml_chunksize
{
    int		mlcs_numlines;
    long	mlcs_totalsize;
} chunksize_T;


# define ML_CHNK_ADDLINE 1
# define ML_CHNK_DELLINE 2
# define ML_CHNK_UPDLINE 3
#endif


typedef struct memline
{
    linenr_T	ml_line_count;	

    memfile_T	*ml_mfp;	

    infoptr_T	*ml_stack;	
    int		ml_stack_top;	
    int		ml_stack_size;	

#define ML_EMPTY	0x01	
#define ML_LINE_DIRTY	0x02	
#define ML_LOCKED_DIRTY	0x04	
#define ML_LOCKED_POS	0x08	
#define ML_ALLOCATED	0x10	
    int		ml_flags;

    colnr_T	ml_line_len;	
    linenr_T	ml_line_lnum;	
    char_u	*ml_line_ptr;	

    bhdr_T	*ml_locked;	
    linenr_T	ml_locked_low;	
    linenr_T	ml_locked_high;	
    int		ml_locked_lineadd;  
#ifdef FEAT_BYTEOFF
    chunksize_T *ml_chunksize;
    int		ml_numchunks;
    int		ml_usedchunks;
#endif
} memline_T;


#define ML_DEL_MESSAGE	    1	
#define ML_DEL_UNDO	    2	
#define ML_DEL_NOPROP	    4	


#define ML_APPEND_NEW	    1	
#define ML_APPEND_MARK	    2	
#define ML_APPEND_UNDO	    4	
#define ML_APPEND_NOPROP    8	



typedef struct textprop_S
{
    colnr_T	tp_col;		
    colnr_T	tp_len;		
				
    int		tp_id;		
    int		tp_type;	
    int		tp_flags;	
    int		tp_padleft;	
				
} textprop_T;

#define TP_FLAG_CONT_NEXT	0x1	
#define TP_FLAG_CONT_PREV	0x2	


#define TP_FLAG_ALIGN_RIGHT	0x010	
#define TP_FLAG_ALIGN_ABOVE	0x020	
#define TP_FLAG_ALIGN_BELOW	0x040	

#define TP_FLAG_WRAP		0x080	
					
#define TP_FLAG_START_INCL	0x100	

#define PROP_TEXT_MIN_CELLS	4	
					


typedef struct proptype_S
{
    int		pt_id;		
    int		pt_type;	
    int		pt_hl_id;	
    int		pt_priority;	
    int		pt_flags;	
    char_u	pt_name[1];	
} proptype_T;

#define PT_FLAG_INS_START_INCL	1	
#define PT_FLAG_INS_END_INCL	2	
#define PT_FLAG_COMBINE		4	
#define PT_FLAG_OVERRIDE	8	


typedef struct signgroup_S
{
    int		sg_next_sign_id;	
    short_u	sg_refcount;		
    char_u	sg_name[1];		
} signgroup_T;

typedef struct sign_entry sign_entry_T;
struct sign_entry
{
    int		 se_id;		
    int		 se_typenr;	
    int		 se_priority;	
    linenr_T	 se_lnum;	
    signgroup_T	 *se_group;	
    sign_entry_T *se_next;	
    sign_entry_T *se_prev;	
};


typedef struct sign_attrs_S {
    int		sat_typenr;
    void	*sat_icon;
    char_u	*sat_text;
    int		sat_texthl;
    int		sat_linehl;
    int		sat_culhl;
    int		sat_numhl;
    int		sat_priority;
} sign_attrs_T;

#if defined(FEAT_SIGNS) || defined(PROTO)

#define SGN_KEY_OFF	offsetof(signgroup_T, sg_name)
#define HI2SG(hi)	((signgroup_T *)((hi)->hi_key - SGN_KEY_OFF))


#define SIGN_DEF_PRIO	10

#endif


typedef struct arglist
{
    garray_T	al_ga;		
    int		al_refcount;	
    int		id;		
} alist_T;


typedef struct argentry
{
    char_u	*ae_fname;	
    int		ae_fnum;	
} aentry_T;

#define ALIST(win)	(win)->w_alist
#define GARGLIST	((aentry_T *)global_alist.al_ga.ga_data)
#define ARGLIST		((aentry_T *)ALIST(curwin)->al_ga.ga_data)
#define WARGLIST(wp)	((aentry_T *)ALIST(wp)->al_ga.ga_data)
#define AARGLIST(al)	((aentry_T *)((al)->al_ga.ga_data))
#define GARGCOUNT	(global_alist.al_ga.ga_len)
#define ARGCOUNT	(ALIST(curwin)->al_ga.ga_len)
#define WARGCOUNT(wp)	(ALIST(wp)->al_ga.ga_len)



typedef struct eslist_elem eslist_T;
struct eslist_elem
{
    int		saved_emsg_silent;	
    eslist_T	*next;			
};


#define CSTACK_LEN	50

typedef struct {
    short	cs_flags[CSTACK_LEN];	
    char	cs_pending[CSTACK_LEN];	
    union {
	void	*csp_rv[CSTACK_LEN];	
	void	*csp_ex[CSTACK_LEN];	
    }		cs_pend;
    void	*cs_forinfo[CSTACK_LEN]; 
    int		cs_line[CSTACK_LEN];	
    int		cs_block_id[CSTACK_LEN];    
    int		cs_script_var_len[CSTACK_LEN];	
						
    int		cs_idx;			
    int		cs_looplevel;		
    int		cs_trylevel;		
    eslist_T	*cs_emsg_silent_list;	
    char	cs_lflags;		
} cstack_T;
# define cs_rettv	cs_pend.csp_rv
# define cs_exception	cs_pend.csp_ex



# define CSF_TRUE	0x0001	
# define CSF_ACTIVE	0x0002	
# define CSF_ELSE	0x0004	
# define CSF_WHILE	0x0008	
# define CSF_FOR	0x0010	
# define CSF_BLOCK	0x0020	

# define CSF_TRY	0x0100	
# define CSF_FINALLY	0x0200	
# define CSF_CATCH	0x0400	
# define CSF_THROWN	0x0800	
# define CSF_CAUGHT	0x1000  
# define CSF_FINISHED	0x2000  
# define CSF_SILENT	0x4000	



# define CSF_FUNC_DEF	0x8000	


# define CSTP_NONE	0	
# define CSTP_ERROR	1	
# define CSTP_INTERRUPT	2	
# define CSTP_THROW	4	
# define CSTP_BREAK	8	
# define CSTP_CONTINUE	16	
# define CSTP_RETURN	24	
# define CSTP_FINISH	32	


# define CSL_HAD_LOOP	 1	
# define CSL_HAD_ENDLOOP 2	
# define CSL_HAD_CONT	 4	
# define CSL_HAD_FINA	 8	


typedef struct msglist msglist_T;
struct msglist
{
    msglist_T	*next;		
    char	*msg;		
    char	*throw_msg;	
    char_u	*sfile;		
    long	slnum;		
    int		msg_compiling;	
};


typedef enum
{
    ET_USER,		
    ET_ERROR,		
    ET_INTERRUPT,	
} except_type_T;


typedef struct vim_exception except_T;
struct vim_exception
{
    except_type_T	type;		
    char		*value;		
    struct msglist	*messages;	
    char_u		*throw_name;	
    linenr_T		throw_lnum;	
    except_T		*caught;	
};


typedef struct cleanup_stuff cleanup_T;
struct cleanup_stuff
{
    int pending;		
    except_T *exception;	
};


typedef struct exception_state_S exception_state_T;
struct exception_state_S
{
    except_T	*estate_current_exception;
    int		estate_did_throw;
    int		estate_need_rethrow;
    int		estate_trylevel;
    int		estate_did_emsg;
};

#ifdef FEAT_SYN_HL

struct sp_syn
{
    int		inc_tag;	
    short	id;		
    short	*cont_in_list;	
};


typedef struct keyentry keyentry_T;

struct keyentry
{
    keyentry_T	*ke_next;	
    struct sp_syn k_syn;	
    short	*next_list;	
    int		flags;
    int		k_char;		
    char_u	keyword[1];	
};


typedef struct buf_state
{
    int		    bs_idx;	 
    int		    bs_flags;	 
#ifdef FEAT_CONCEAL
    int		    bs_seqnr;	 
    int		    bs_cchar;	 
#endif
    reg_extmatch_T *bs_extmatch; 
} bufstate_T;


typedef struct syn_state synstate_T;

struct syn_state
{
    synstate_T	*sst_next;	
    linenr_T	sst_lnum;	
    union
    {
	bufstate_T	sst_stack[SST_FIX_STATES]; 
	garray_T	sst_ga;	
    } sst_union;
    int		sst_next_flags;	
    int		sst_stacksize;	
    short	*sst_next_list;	
				
    disptick_T	sst_tick;	
    linenr_T	sst_change_lnum;
				
};
#endif 

#define MAX_HL_ID       20000	


typedef struct attr_entry
{
    short	    ae_attr;		
    union
    {
	struct
	{
	    char_u	    *start;	
	    char_u	    *stop;	
	} term;
	struct
	{
	    
	    short_u	    fg_color;	
	    short_u	    bg_color;	
	    short_u	    ul_color;	
# ifdef FEAT_TERMGUICOLORS
	    guicolor_T	    fg_rgb;	
	    guicolor_T	    bg_rgb;	
	    guicolor_T	    ul_rgb;	
# endif
	} cterm;
# ifdef FEAT_GUI
	struct
	{
	    guicolor_T	    fg_color;	
	    guicolor_T	    bg_color;	
	    guicolor_T	    sp_color;	
	    GuiFont	    font;	
#  ifdef FEAT_XFONTSET
	    GuiFontset	    fontset;	
#  endif
	} gui;
# endif
    } ae_u;
} attrentry_T;

#ifdef USE_ICONV
# ifdef HAVE_ICONV_H
#  include <iconv.h>
# else
#  if defined(MACOS_X)
#   include <sys/errno.h>
#   ifndef EILSEQ
#    define EILSEQ ENOENT 
#   endif
typedef struct _iconv_t *iconv_t;
#  else
#   include <errno.h>
#  endif
typedef void *iconv_t;
# endif
#endif


typedef struct
{
    char_u	*tb_buf;	
    char_u	*tb_noremap;	
    int		tb_buflen;	
    int		tb_off;		
    int		tb_len;		
    int		tb_maplen;	
    int		tb_silent;	
    int		tb_no_abbr_cnt; 
    int		tb_change_cnt;	
} typebuf_T;


typedef struct
{
    typebuf_T		save_typebuf;
    int			typebuf_valid;	    
    int			old_char;
    int			old_mod_mask;
    buffheader_T	save_readbuf1;
    buffheader_T	save_readbuf2;
#ifdef USE_INPUT_BUF
    char_u		*save_inputbuf;
#endif
} tasave_T;


typedef struct
{
    int		vc_type;	
    int		vc_factor;	
# ifdef MSWIN
    int		vc_cpfrom;	
    int		vc_cpto;	
# endif
# ifdef USE_ICONV
    iconv_t	vc_fd;		
# endif
    int		vc_fail;	
} vimconv_T;


typedef struct hist_entry
{
    int		hisnum;		
    int		viminfo;	
    char_u	*hisstr;	
    time_t	time_set;	
} histentry_T;

#define CONV_NONE		0
#define CONV_TO_UTF8		1
#define CONV_9_TO_UTF8		2
#define CONV_TO_LATIN1		3
#define CONV_TO_LATIN9		4
#define CONV_ICONV		5
#ifdef MSWIN
# define CONV_CODEPAGE		10	
#endif
#ifdef MACOS_X
# define CONV_MAC_LATIN1	20
# define CONV_LATIN1_MAC	21
# define CONV_MAC_UTF8		22
# define CONV_UTF8_MAC		23
#endif


typedef struct mapblock mapblock_T;
struct mapblock
{
    mapblock_T	*m_next;	
    char_u	*m_keys;	
    char_u	*m_str;		
    char_u	*m_orig_str;	
    int		m_keylen;	
    int		m_mode;		
    int		m_simplified;	
				
    int		m_noremap;	
    char	m_silent;	
    char	m_nowait;	
#ifdef FEAT_EVAL
    char	m_expr;		
    sctx_T	m_script_ctx;	
#endif
};



typedef struct
{
    char_u	*start;
    int		userhl;		
} stl_hlrec_T;





typedef struct hashitem_S
{
    long_u	hi_hash;	
    char_u	*hi_key;
} hashitem_T;



#define HI_KEY_REMOVED &hash_removed
#define HASHITEM_EMPTY(hi) ((hi)->hi_key == NULL || (hi)->hi_key == &hash_removed)




#define HT_INIT_SIZE 16


#define HTFLAGS_ERROR	0x01	
				
#define HTFLAGS_FROZEN	0x02	
				

typedef struct hashtable_S
{
    long_u	ht_mask;	
				
    long_u	ht_used;	
    long_u	ht_filled;	
    int		ht_changed;	
    int		ht_locked;	
    int		ht_flags;	
    hashitem_T	*ht_array;	
				
    hashitem_T	ht_smallarray[HT_INIT_SIZE];   
} hashtab_T;

typedef long_u hash_T;		



#ifdef MSWIN
# ifdef PROTO
   
   typedef long			varnumber_T;
   typedef unsigned long	uvarnumber_T;
#  define VARNUM_MIN		LONG_MIN
#  define VARNUM_MAX		LONG_MAX
#  define UVARNUM_MAX		ULONG_MAX
# else
   typedef __int64		varnumber_T;
   typedef unsigned __int64	uvarnumber_T;
#  define VARNUM_MIN		_I64_MIN
#  define VARNUM_MAX		_I64_MAX
#  define UVARNUM_MAX		_UI64_MAX
# endif
#elif defined(HAVE_NO_LONG_LONG)
# if defined(HAVE_STDINT_H)
   typedef int64_t		varnumber_T;
   typedef uint64_t		uvarnumber_T;
#  define VARNUM_MIN		INT64_MIN
#  define VARNUM_MAX		INT64_MAX
#  define UVARNUM_MAX		UINT64_MAX
# else
   
   typedef long			varnumber_T;
   typedef unsigned long	uvarnumber_T;
#  define VARNUM_MIN		LONG_MIN
#  define VARNUM_MAX		LONG_MAX
#  define UVARNUM_MAX		ULONG_MAX
# endif
#else
  typedef long long		varnumber_T;
  typedef unsigned long long	uvarnumber_T;
# ifdef LLONG_MIN
#  define VARNUM_MIN		LLONG_MIN
#  define VARNUM_MAX		LLONG_MAX
#  define UVARNUM_MAX		ULLONG_MAX
# else
#  define VARNUM_MIN		LONG_LONG_MIN
#  define VARNUM_MAX		LONG_LONG_MAX
#  define UVARNUM_MAX		ULONG_LONG_MAX
# endif
#endif



typedef signed char int8_T;

typedef double	float_T;

typedef struct typval_S typval_T;
typedef struct listvar_S list_T;
typedef struct dictvar_S dict_T;
typedef struct partial_S partial_T;
typedef struct blobvar_S blob_T;






typedef struct {
    char_u	*cb_name;
    partial_T	*cb_partial;
    int		cb_free_name;	    
} callback_T;

typedef struct isn_S isn_T;	    
typedef struct dfunc_S dfunc_T;	    

typedef struct type_S type_T;
typedef struct ufunc_S ufunc_T;

typedef struct jobvar_S job_T;
typedef struct readq_S readq_T;
typedef struct writeq_S writeq_T;
typedef struct jsonq_S jsonq_T;
typedef struct cbq_S cbq_T;
typedef struct channel_S channel_T;
typedef struct cctx_S cctx_T;
typedef struct ectx_S ectx_T;
typedef struct instr_S instr_T;
typedef struct class_S class_T;
typedef struct object_S object_T;
typedef struct typealias_S typealias_T;

typedef enum
{
    VAR_UNKNOWN = 0,	
    VAR_ANY,		
    VAR_VOID,		
    VAR_BOOL,		
    VAR_SPECIAL,	
    VAR_NUMBER,		
    VAR_FLOAT,		
    VAR_STRING,		
    VAR_BLOB,		
    VAR_FUNC,		
    VAR_PARTIAL,	
    VAR_LIST,		
    VAR_DICT,		
    VAR_JOB,		
    VAR_CHANNEL,	
    VAR_INSTR,		
    VAR_CLASS,		
    VAR_OBJECT,		
    VAR_TYPEALIAS	
} vartype_T;


struct type_S {
    vartype_T	    tt_type;
    int8_T	    tt_argcount;    
    int8_T	    tt_min_argcount; 
    char_u	    tt_flags;	    
    type_T	    *tt_member;	    
    class_T	    *tt_class;	    
    type_T	    **tt_args;	    
};

typedef struct {
    type_T	*type_curr;	    
    type_T	*type_decl;	    
} type2_T;

#define TTFLAG_VARARGS	    0x01    
#define TTFLAG_BOOL_OK	    0x02    
#define TTFLAG_FLOAT_OK	    0x04    
#define TTFLAG_NUMBER_OK    0x08    
#define TTFLAG_STATIC	    0x10    
#define TTFLAG_CONST	    0x20    
#define TTFLAG_SUPER	    0x40    

typedef enum {
    VIM_ACCESS_PRIVATE,	
    VIM_ACCESS_READ,	
    VIM_ACCESS_ALL	
} omacc_T;

#define OCMFLAG_HAS_TYPE	0x01	
#define OCMFLAG_FINAL		0x02	
#define OCMFLAG_CONST		0x04	


typedef struct {
    char_u	*ocm_name;	
    omacc_T	ocm_access;
    type_T	*ocm_type;
    int		ocm_flags;
    char_u	*ocm_init;	
} ocmember_T;


typedef struct itf2class_S itf2class_T;
struct itf2class_S {
    itf2class_T	*i2c_next;
    class_T	*i2c_class;
    int		i2c_is_method;	    
    
};

#define CLASS_INTERFACE	    1
#define CLASS_EXTENDED	    2	    
#define CLASS_ABSTRACT	    4	    



struct class_S
{
    char_u	*class_name;		
    int		class_flags;		

    int		class_refcount;
    int		class_copyID;		
    class_T	*class_next_used;	
    class_T	*class_prev_used;	

    class_T	*class_extends;		

    
    int		class_interface_count;
    char_u	**class_interfaces;	
    class_T	**class_interfaces_cl;	
    itf2class_T	*class_itf2class;	

    
    int		class_class_member_count;
    ocmember_T	*class_class_members;	
    typval_T	*class_members_tv;	

    
    int		class_class_function_count;	    
    int		class_class_function_count_child;   
    ufunc_T	**class_class_functions;	

    
    int		class_obj_member_count;
    ocmember_T	*class_obj_members;	

    
    int		class_obj_method_count;		    
    int		class_obj_method_count_child;	    
    ufunc_T	**class_obj_methods;	

    garray_T	class_type_list;	
    type_T	class_type;		
    type_T	class_object_type;	
};



struct object_S
{
    class_T	*obj_class;	    
				    
    int		obj_refcount;

    object_T	*obj_next_used;	    
    object_T	*obj_prev_used;	    
    int		obj_copyID;	    
};

struct typealias_S
{
    int	    ta_refcount;
    type_T  *ta_type;
    char_u  *ta_name;
};


struct typval_S
{
    vartype_T	v_type;
    char	v_lock;	    
    union
    {
	varnumber_T	v_number;	
	float_T		v_float;	
	char_u		*v_string;	
	list_T		*v_list;	
	dict_T		*v_dict;	
	partial_T	*v_partial;	
#ifdef FEAT_JOB_CHANNEL
	job_T		*v_job;		
	channel_T	*v_channel;	
#endif
	blob_T		*v_blob;	
	instr_T		*v_instr;	
	class_T		*v_class;	
	object_T	*v_object;	
	typealias_T	*v_typealias;	
    }		vval;
};


#define VAR_SCOPE     1	
#define VAR_DEF_SCOPE 2	
			


#define VAR_LOCKED	    1	
#define VAR_FIXED	    2	
#define VAR_ITEMS_LOCKED    4	


typedef struct listitem_S listitem_T;

struct listitem_S
{
    listitem_T	*li_next;	
    listitem_T	*li_prev;	
    typval_T	li_tv;		
};


typedef struct listwatch_S listwatch_T;

struct listwatch_S
{
    listitem_T		*lw_item;	
    listwatch_T		*lw_next;	
};


struct listvar_S
{
    listitem_T	*lv_first;	
				
    listwatch_T	*lv_watch;	
    union {
	struct {	
			
	    varnumber_T lv_start;
	    varnumber_T lv_end;
	    int		lv_stride;
	} nonmat;
	struct {	
	    listitem_T	*lv_last;	
	    listitem_T	*lv_idx_item;	
	    int		lv_idx;		
	} mat;
    } lv_u;
    type_T	*lv_type;	
    list_T	*lv_copylist;	
    list_T	*lv_used_next;	
    list_T	*lv_used_prev;	
    int		lv_refcount;	
    int		lv_len;		
    int		lv_with_items;	
				
    int		lv_copyID;	
    char	lv_lock;	
};


typedef struct {
    list_T	sl_list;	
    listitem_T	sl_items[10];
} staticList10_T;


struct dictitem_S
{
    typval_T	di_tv;		
    char_u	di_flags;	
    char_u	di_key[1];	
};
typedef struct dictitem_S dictitem_T;


#define DICTITEM16_KEY_LEN 16
struct dictitem16_S
{
    typval_T	di_tv;		
    char_u	di_flags;	
    char_u	di_key[DICTITEM16_KEY_LEN + 1];	
};
typedef struct dictitem16_S dictitem16_T;


#define DI_FLAGS_RO	   0x01	    
#define DI_FLAGS_RO_SBX	   0x02	    
#define DI_FLAGS_FIX	   0x04	    
#define DI_FLAGS_LOCK	   0x08	    
#define DI_FLAGS_ALLOC	   0x10	    
#define DI_FLAGS_RELOAD	   0x20	    


struct dictvar_S
{
    char	dv_lock;	
    char	dv_scope;	
    int		dv_refcount;	
    int		dv_copyID;	
    hashtab_T	dv_hashtab;	
    type_T	*dv_type;	
    dict_T	*dv_copydict;	
    dict_T	*dv_used_next;	
    dict_T	*dv_used_prev;	
};


struct blobvar_S
{
    garray_T	bv_ga;		
    int		bv_refcount;	
    char	bv_lock;	
};

typedef int (*cfunc_T)(int argcount, typval_T *argvars, typval_T *rettv, void *state);
typedef void (*cfunc_free_T)(void *state);


typedef enum {
    GETLINE_NONE,	    
    GETLINE_CONCAT_CONT,    
    GETLINE_CONCAT_CONTBAR, 
    GETLINE_CONCAT_ALL	    
} getline_opt_T;

typedef struct svar_S svar_T;

#if defined(FEAT_EVAL) || defined(PROTO)

typedef struct
{
    int		fi_semicolon;	
    int		fi_varcount;	
    int		fi_break_count;	
    listwatch_T	fi_lw;		
    list_T	*fi_list;	
    int		fi_bi;		
    blob_T	*fi_blob;	
    char_u	*fi_string;	
    int		fi_byte_idx;	
    int		fi_cs_flags;	
} forinfo_T;

typedef struct funccall_S funccall_T;


typedef enum {
    UF_NOT_COMPILED,	    
    UF_TO_BE_COMPILED,	    
    UF_COMPILING,	    
    UF_COMPILED,	    
    UF_COMPILE_ERROR	    
} def_status_T;


struct ufunc_S
{
    int		uf_varargs;	
    int		uf_flags;	
    int		uf_calls;	
    int		uf_cleared;	
    def_status_T uf_def_status; 
    int		uf_dfunc_idx;	

    class_T	*uf_class;	
				
				
				
    class_T	*uf_defclass;	

    garray_T	uf_args;	
    garray_T	uf_def_args;	
    int		uf_args_visible; 
				 

    
    type_T	**uf_arg_types;	
    type_T	*uf_ret_type;	
    garray_T	uf_type_list;	
    partial_T	*uf_partial;	
				

    char_u	*uf_va_name;	
    type_T	*uf_va_type;	
    type_T	*uf_func_type;	
    int		uf_block_depth;	
    int		*uf_block_ids;	
# if defined(FEAT_LUA)
    cfunc_T     uf_cb;		
    cfunc_free_T uf_cb_free;    
    void	*uf_cb_state;   
# endif

    garray_T	uf_lines;	

    int		uf_debug_tick;	
				
    int		uf_has_breakpoint;  
				    
# ifdef FEAT_PROFILE
    int		uf_profiling;	
    int		uf_prof_initialized;
    hash_T	uf_hash;	
    
    int		uf_tm_count;	
    proftime_T	uf_tm_total;	
    proftime_T	uf_tm_self;	
    proftime_T	uf_tm_children;	
    
    int		*uf_tml_count;	
    proftime_T	*uf_tml_total;	
    proftime_T	*uf_tml_self;	
    proftime_T	uf_tml_start;	
    proftime_T	uf_tml_children; 
    proftime_T	uf_tml_wait;	
    int		uf_tml_idx;	
    int		uf_tml_execed;	
# endif
    sctx_T	uf_script_ctx;	
				
				
    int		uf_script_ctx_version;  
    int		uf_refcount;	

    funccall_T	*uf_scoped;	

    char_u	*uf_name_exp;	
				
    char_u	uf_name[4];	
				
				
};


#define FC_ABORT    0x01	
#define FC_RANGE    0x02	
#define FC_DICT	    0x04	
#define FC_CLOSURE  0x08	
#define FC_DELETED  0x10	
#define FC_REMOVED  0x20	
#define FC_SANDBOX  0x40	
#define FC_DEAD	    0x80	
#define FC_EXPORT   0x100	
#define FC_NOARGS   0x200	
#define FC_VIM9	    0x400	
#define FC_CFUNC    0x800	
#define FC_COPY	    0x1000	
				
#define FC_LAMBDA   0x2000	

#define FC_OBJECT   0x4000	
#define FC_NEW	    0x8000	
#define FC_ABSTRACT 0x10000	


#define IS_OBJECT_METHOD(ufunc) ((ufunc->uf_flags & FC_OBJECT) == FC_OBJECT)

#define IS_CONSTRUCTOR_METHOD(ufunc) ((ufunc->uf_flags & FC_NEW) == FC_NEW)

#define IS_ABSTRACT_METHOD(ufunc) ((ufunc->uf_flags & FC_ABSTRACT) == FC_ABSTRACT)

#define MAX_FUNC_ARGS	20	
#define VAR_SHORT_LEN	20	
#define FIXVAR_CNT	12	


struct funccall_S
{
    ufunc_T	*fc_func;	
    int		fc_linenr;	
    int		fc_returned;	
    struct			
    {
	dictitem_T	var;		
	char_u	room[VAR_SHORT_LEN];	
    } fc_fixvar[FIXVAR_CNT];
    dict_T	fc_l_vars;	
    dictitem_T	fc_l_vars_var;	
    dict_T	fc_l_avars;	
    dictitem_T	fc_l_avars_var;	
    list_T	fc_l_varlist;	
    listitem_T	fc_l_listitems[MAX_FUNC_ARGS];	
    typval_T	*fc_rettv;	
    linenr_T	fc_breakpoint;	
    int		fc_dbg_tick;	
    int		fc_level;	

    garray_T	fc_defer;	
    ectx_T	*fc_ectx;	
				

#ifdef FEAT_PROFILE
    proftime_T	fc_prof_child;	
#endif
    funccall_T	*fc_caller;	
				

    
    int		fc_refcount;	
				
    int		fc_copyID;	
    garray_T	fc_ufuncs;	
				
};


typedef struct
{
    char_u	*dr_name;	
    typval_T	dr_argvars[MAX_FUNC_ARGS + 1];
    int		dr_argcount;
} defer_T;


typedef struct
{
    dict_T	*fd_dict;	
    char_u	*fd_newkey;	
    dictitem_T	*fd_di;		
} funcdict_T;

typedef struct funccal_entry funccal_entry_T;
struct funccal_entry {
    void	    *top_funccal;
    funccal_entry_T *next;
};


#define UF2HIKEY(fp) ((fp)->uf_name)
#define HIKEY2UF(p)  ((ufunc_T *)((p) - offsetof(ufunc_T, uf_name)))
#define HI2UF(hi)     HIKEY2UF((hi)->hi_key)


typedef struct {
    dictitem_T	sv_var;
    dict_T	sv_dict;
} scriptvar_T;


typedef struct sallvar_S sallvar_T;
struct sallvar_S {
    sallvar_T	*sav_next;	  
    int		sav_block_id;	  
    int		sav_var_vals_idx; 

    
    
    
    dictitem_T *sav_di;		
    typval_T	sav_tv;		
    char_u	sav_flags;	
    char_u	sav_key[1];	
};


#define SAV2HIKEY(sav) ((sav)->sav_key)
#define HIKEY2SAV(p)  ((sallvar_T *)(p - offsetof(sallvar_T, sav_key)))
#define HI2SAV(hi)     HIKEY2SAV((hi)->hi_key)

#define SVFLAG_TYPE_ALLOCATED	1  
#define SVFLAG_EXPORTED		2  
#define SVFLAG_ASSIGNED		4  


struct svar_S {
    char_u	*sv_name;	
    typval_T	*sv_tv;		
    type_T	*sv_type;
    int		sv_flags;	
    int		sv_const;	
};

typedef struct {
    char_u	*imp_name;	    
    scid_T	imp_sid;	    
    int		imp_flags;	    
} imported_T;

#define IMP_FLAGS_RELOAD	2   
#define IMP_FLAGS_AUTOLOAD	4   


typedef struct
{
    char_u	*sn_name;	    
    int		sn_script_seq;	    

    
    
    
    int		sn_sourced_sid;

    
    
    scriptvar_T	*sn_vars;

    
    
    
    
    
    
    
    dict_T	sn_all_vars;	

    
    
    
    garray_T	sn_var_vals;

    garray_T	sn_imports;	
    garray_T	sn_type_list;	
    int		sn_current_block_id; 
    int		sn_last_block_id;  

    int		sn_version;	
    int		sn_state;	
    char_u	*sn_save_cpo;	
    char	sn_is_vimrc;	

    
    char_u	*sn_autoload_prefix;

    
    
    int		sn_import_autoload;

# ifdef FEAT_PROFILE
    int		sn_prof_on;	
    int		sn_pr_force;	
    proftime_T	sn_pr_child;	
    int		sn_pr_nest;	
    
    int		sn_pr_count;	
    proftime_T	sn_pr_total;	
    proftime_T	sn_pr_self;	
    proftime_T	sn_pr_start;	
    proftime_T	sn_pr_children; 
    
    garray_T	sn_prl_ga;	
    proftime_T	sn_prl_start;	
    proftime_T	sn_prl_children; 
    proftime_T	sn_prl_wait;	
    int		sn_prl_idx;	
    int		sn_prl_execed;	
# endif
} scriptitem_T;

#define SN_STATE_NEW		0   
#define SN_STATE_NOT_LOADED	1   
#define SN_STATE_RELOAD		2   
#define SN_STATE_HAD_COMMAND	9   



typedef struct {
    int		eval_flags;	    
    int		eval_break_count;   

    
    char_u	*(*eval_getline)(int, void *, int, getline_opt_T);
    void	*eval_cookie;	    

    
    cctx_T	*eval_cctx;

    
    cstack_T	*eval_cstack;

    
    
    
    
    garray_T	eval_ga;
    garray_T	eval_freega;

    
    char_u	*eval_tofree;

    
    garray_T	eval_tofree_ga;

    
    int		eval_using_cmdline;

    
    char_u	*eval_tofree_lambda;
} evalarg_T;


#define EVAL_EVALUATE	    1	    

# ifdef FEAT_PROFILE

typedef struct sn_prl_S
{
    int		snp_count;	
    proftime_T	sn_prl_total;	
    proftime_T	sn_prl_self;	
} sn_prl_T;

#  define PRL_ITEM(si, idx)	(((sn_prl_T *)(si)->sn_prl_ga.ga_data)[(idx)])

typedef struct {
    int		pi_started_profiling;
    proftime_T	pi_wait_start;
    proftime_T	pi_call_start;
} profinfo_T;

# else
typedef struct
{
    int	    dummy;
} profinfo_T;
# endif
#else

struct ufunc_S
{
    int	    dummy;
};
typedef struct
{
    int	    dummy;
} funccall_T;
typedef struct
{
    int	    dummy;
} funcdict_T;
typedef struct
{
    int	    dummy;
} funccal_entry_T;
typedef struct
{
    int	    dummy;
} scriptitem_T;
typedef struct
{
    int	    dummy;
} evalarg_T;
#endif








typedef struct {
    int		(* fe_argv_func)(int, typval_T *, int, ufunc_T *);
    linenr_T	fe_firstline;	
    linenr_T	fe_lastline;	
    int		*fe_doesrange;	
    int		fe_evaluate;	
    ufunc_T	*fe_ufunc;	
				
    partial_T	*fe_partial;	
    dict_T	*fe_selfdict;	
    object_T	*fe_object;	
    typval_T	*fe_basetv;	
    type_T	*fe_check_type;	
    int		fe_found_var;	
				
} funcexe_T;


typedef struct funcstack_S funcstack_T;
struct funcstack_S
{
    funcstack_T *fs_next;	
    funcstack_T *fs_prev;

    garray_T	fs_ga;		
				
				
				
    int		fs_var_offset;	
				

    int		fs_refcount;	
    int		fs_min_refcount; 
    int		fs_copyID;	
};


typedef struct loopvars_S loopvars_T;
struct loopvars_S
{
    loopvars_T *lvs_next;	
    loopvars_T *lvs_prev;

    garray_T	lvs_ga;		
    int		lvs_refcount;	
    int		lvs_min_refcount; 
    int		lvs_copyID;	
};


#define MAX_LOOP_DEPTH 10

typedef struct outer_S outer_T;
struct outer_S {
    garray_T	*out_stack;	    
				    
    int		out_frame_idx;	    
    outer_T	*out_up;	    
    partial_T	*out_up_partial;    

    struct {
	garray_T *stack;	    
				    
	short	 var_idx;	    
				    
	short	 var_count;	    
    } out_loop[MAX_LOOP_DEPTH];
    int		out_loop_size;	    
};

struct partial_S
{
    int		pt_refcount;	
    int		pt_auto;	
				
    char_u	*pt_name;	
				
    ufunc_T	*pt_func;	
				

    
    outer_T	pt_outer;

    
    partial_T	*pt_outer_partial;

    funcstack_T	*pt_funcstack;	
				
    loopvars_T	*(pt_loopvars[MAX_LOOP_DEPTH]);
				
				

    typval_T	*pt_argv;	
    int		pt_argc;	

    int		pt_copyID;	
    dict_T	*pt_dict;	
    object_T	*pt_obj;	
};

typedef struct {
    short	lvi_depth;	    
    struct {
	short	var_idx;	    
	short	var_count;	    
    } lvi_loop[MAX_LOOP_DEPTH];
} loopvarinfo_T;

typedef struct AutoPatCmd_S AutoPatCmd_T;


typedef enum {
    ETYPE_TOP,		    
    ETYPE_SCRIPT,	    
    ETYPE_UFUNC,	    
    ETYPE_AUCMD,	    
    ETYPE_MODELINE,	    
    ETYPE_EXCEPT,	    
    ETYPE_ARGS,		    
    ETYPE_ENV,		    
    ETYPE_INTERNAL,	    
    ETYPE_SPELL,	    
} etype_T;

typedef struct {
    long      es_lnum;      
    char_u    *es_name;     
    etype_T   es_type;
    union {
	sctx_T  *sctx;      
#if defined(FEAT_EVAL)
	ufunc_T *ufunc;     
#endif
	AutoPatCmd_T *aucmd;  
	except_T   *except; 
    } es_info;
#if defined(FEAT_EVAL)
    sctx_T	es_save_sctx;	    
#endif
} estack_T;


typedef struct {
    int backspace;	
    int enter;		
    int interrupt;	
    int nl_does_cr;	
} ttyinfo_T;


typedef enum
{
    JOB_FAILED,
    JOB_STARTED,
    JOB_ENDED,	    
    JOB_FINISHED,   
} jobstatus_T;


struct jobvar_S
{
    job_T	*jv_next;
    job_T	*jv_prev;
#ifdef UNIX
    pid_t	jv_pid;
#endif
#ifdef MSWIN
    PROCESS_INFORMATION	jv_proc_info;
    HANDLE		jv_job_object;
#endif
    jobstatus_T	jv_status;
    char_u	*jv_tty_in;	
    char_u	*jv_tty_out;	
    char_u	*jv_stoponexit;	
#ifdef UNIX
    char_u	*jv_termsig;	
#endif
#ifdef MSWIN
    char_u	*jv_tty_type;	
#endif
    int		jv_exitval;
    callback_T	jv_exit_cb;

    buf_T	*jv_in_buf;	

    int		jv_refcount;	
    int		jv_copyID;

    channel_T	*jv_channel;	
    char	**jv_argv;	
};


struct readq_S
{
    char_u	*rq_buffer;
    long_u	rq_buflen;
    readq_T	*rq_next;
    readq_T	*rq_prev;
};

struct writeq_S
{
    garray_T	wq_ga;
    writeq_T	*wq_next;
    writeq_T	*wq_prev;
};

struct jsonq_S
{
    typval_T	*jq_value;
    jsonq_T	*jq_next;
    jsonq_T	*jq_prev;
    int		jq_no_callback; 
};

struct cbq_S
{
    callback_T	cq_callback;
    int		cq_seq_nr;
    cbq_T	*cq_next;
    cbq_T	*cq_prev;
};


typedef enum
{
    CH_MODE_NL = 0,
    CH_MODE_RAW,
    CH_MODE_JSON,
    CH_MODE_JS,
    CH_MODE_LSP		
} ch_mode_T;

typedef enum {
    JIO_PIPE,	    
    JIO_NULL,
    JIO_FILE,
    JIO_BUFFER,
    JIO_OUT
} job_io_T;

#define CH_PART_FD(part)	ch_part[part].ch_fd



typedef enum {
    PART_SOCK = 0,
#define CH_SOCK_FD	CH_PART_FD(PART_SOCK)
#ifdef FEAT_JOB_CHANNEL
    PART_OUT,
# define CH_OUT_FD	CH_PART_FD(PART_OUT)
    PART_ERR,
# define CH_ERR_FD	CH_PART_FD(PART_ERR)
    PART_IN,
# define CH_IN_FD	CH_PART_FD(PART_IN)
#endif
    PART_COUNT,
} ch_part_T;

#define INVALID_FD	(-1)


typedef struct {
    sock_T	ch_fd;	    

# if defined(UNIX) && !defined(HAVE_SELECT)
    int		ch_poll_idx;	
# endif

#ifdef FEAT_GUI_X11
    XtInputId	ch_inputHandler; 
#endif
#ifdef FEAT_GUI_GTK
    gint	ch_inputHandler; 
#endif

    ch_mode_T	ch_mode;
    job_io_T	ch_io;
    int		ch_timeout;	

    readq_T	ch_head;	
    jsonq_T	ch_json_head;	
    garray_T	ch_block_ids;	
				
    
    
    
    
    size_t	ch_wait_len;
#ifdef MSWIN
    DWORD	ch_deadline;
#else
    struct timeval ch_deadline;
#endif
    int		ch_block_write;	
				
    int		ch_nonblocking;	
    writeq_T	ch_writeque;	

    cbq_T	ch_cb_head;	
    callback_T	ch_callback;	

    bufref_T	ch_bufref;	
    int		ch_nomodifiable; 
    int		ch_nomod_error;	
    int		ch_buf_append;	
    linenr_T	ch_buf_top;	
    linenr_T	ch_buf_bot;	
} chanpart_T;

struct channel_S {
    channel_T	*ch_next;
    channel_T	*ch_prev;

    int		ch_id;		
    int		ch_last_msg_id;	

    chanpart_T	ch_part[PART_COUNT]; 
    int		ch_write_text_mode; 

    char	*ch_hostname;	
    int		ch_port;	

    int		ch_to_be_closed; 
				 
				 
    int		ch_to_be_freed; 
				
    int		ch_error;	
				
				
				
				

    void	(*ch_nb_close_cb)(void);
				
				

#ifdef MSWIN
    int		ch_named_pipe;	
#endif
    callback_T	ch_callback;	
    callback_T	ch_close_cb;	
    int		ch_drop_never;
    int		ch_keep_open;	
    int		ch_nonblock;

    job_T	*ch_job;	
				
				
    int		ch_job_killed;	
				
    int		ch_anonymous_pipe;  
    int		ch_killing;	    

    int		ch_refcount;	
    int		ch_copyID;
};

#define JO_MODE		    0x0001	
#define JO_IN_MODE	    0x0002	
#define JO_OUT_MODE	    0x0004	
#define JO_ERR_MODE	    0x0008	
#define JO_CALLBACK	    0x0010	
#define JO_OUT_CALLBACK	    0x0020	
#define JO_ERR_CALLBACK	    0x0040	
#define JO_CLOSE_CALLBACK   0x0080	
#define JO_WAITTIME	    0x0100	
#define JO_TIMEOUT	    0x0200	
#define JO_OUT_TIMEOUT	    0x0400	
#define JO_ERR_TIMEOUT	    0x0800	
#define JO_PART		    0x1000	
#define JO_ID		    0x2000	
#define JO_STOPONEXIT	    0x4000	
#define JO_EXIT_CB	    0x8000	
#define JO_OUT_IO	    0x10000	
#define JO_ERR_IO	    0x20000	
#define JO_IN_IO	    0x40000	
#define JO_OUT_NAME	    0x80000	
#define JO_ERR_NAME	    0x100000	
#define JO_IN_NAME	    0x200000	
#define JO_IN_TOP	    0x400000	
#define JO_IN_BOT	    0x800000	
#define JO_OUT_BUF	    0x1000000	
#define JO_ERR_BUF	    0x2000000	
#define JO_IN_BUF	    0x4000000	
#define JO_CHANNEL	    0x8000000	
#define JO_BLOCK_WRITE	    0x10000000	
#define JO_OUT_MODIFIABLE   0x20000000	
#define JO_ERR_MODIFIABLE   0x40000000	
#define JO_ALL		    0x7fffffff

#define JO2_OUT_MSG	    0x0001	
#define JO2_ERR_MSG	    0x0002	
#define JO2_TERM_NAME	    0x0004	
#define JO2_TERM_FINISH	    0x0008	
#define JO2_ENV		    0x0010	
#define JO2_CWD		    0x0020	
#define JO2_TERM_ROWS	    0x0040	
#define JO2_TERM_COLS	    0x0080	
#define JO2_VERTICAL	    0x0100	
#define JO2_CURWIN	    0x0200	
#define JO2_HIDDEN	    0x0400	
#define JO2_TERM_OPENCMD    0x0800	
#define JO2_EOF_CHARS	    0x1000	
#define JO2_NORESTORE	    0x2000	
#define JO2_TERM_KILL	    0x4000	
#define JO2_ANSI_COLORS	    0x8000	
#define JO2_TTY_TYPE	    0x10000	
#define JO2_BUFNR	    0x20000	
#define JO2_TERM_API	    0x40000	
#define JO2_TERM_HIGHLIGHT  0x80000	

#define JO_MODE_ALL	(JO_MODE + JO_IN_MODE + JO_OUT_MODE + JO_ERR_MODE)
#define JO_CB_ALL \
    (JO_CALLBACK + JO_OUT_CALLBACK + JO_ERR_CALLBACK + JO_CLOSE_CALLBACK)
#define JO_TIMEOUT_ALL	(JO_TIMEOUT + JO_OUT_TIMEOUT + JO_ERR_TIMEOUT)


typedef struct
{
    int		jo_set;		
    int		jo_set2;	

    ch_mode_T	jo_mode;
    ch_mode_T	jo_in_mode;
    ch_mode_T	jo_out_mode;
    ch_mode_T	jo_err_mode;
    int		jo_noblock;

    job_io_T	jo_io[4];	
    char_u	jo_io_name_buf[4][NUMBUFLEN];
    char_u	*jo_io_name[4];	
    int		jo_io_buf[4];
    int		jo_pty;
    int		jo_modifiable[4];
    int		jo_message[4];
    channel_T	*jo_channel;

    linenr_T	jo_in_top;
    linenr_T	jo_in_bot;

    callback_T	jo_callback;
    callback_T	jo_out_cb;
    callback_T	jo_err_cb;
    callback_T	jo_close_cb;
    callback_T	jo_exit_cb;
    int		jo_drop_never;
    int		jo_waittime;
    int		jo_timeout;
    int		jo_out_timeout;
    int		jo_err_timeout;
    int		jo_block_write;	
    int		jo_part;
    int		jo_id;
    char_u	jo_stoponexit_buf[NUMBUFLEN];
    char_u	*jo_stoponexit;
    dict_T	*jo_env;	
    char_u	jo_cwd_buf[NUMBUFLEN];
    char_u	*jo_cwd;

#ifdef FEAT_TERMINAL
    
    int		jo_term_rows;
    int		jo_term_cols;
    int		jo_vertical;
    int		jo_curwin;
    buf_T	*jo_bufnr_buf;
    int		jo_hidden;
    int		jo_term_norestore;
    char_u	jo_term_name_buf[NUMBUFLEN];
    char_u	*jo_term_name;
    char_u	jo_term_opencmd_buf[NUMBUFLEN];
    char_u	*jo_term_opencmd;
    int		jo_term_finish;
    char_u	jo_eof_chars_buf[NUMBUFLEN];
    char_u	*jo_eof_chars;
    char_u	jo_term_kill_buf[NUMBUFLEN];
    char_u	*jo_term_kill;
# if defined(FEAT_GUI) || defined(FEAT_TERMGUICOLORS)
    long_u	jo_ansi_colors[16];
# endif
    char_u	jo_term_highlight_buf[NUMBUFLEN];
    char_u	*jo_term_highlight;
    int		jo_tty_type;	    
    char_u	jo_term_api_buf[NUMBUFLEN];
    char_u	*jo_term_api;
#endif
} jobopt_T;

#ifdef FEAT_EVAL

typedef struct listener_S listener_T;
struct listener_S
{
    listener_T	*lr_next;
    int		lr_id;
    callback_T	lr_callback;
};
#endif


typedef struct ht_stack_S
{
    hashtab_T		*ht;
    struct ht_stack_S	*prev;
} ht_stack_T;


typedef struct list_stack_S
{
    list_T		*list;
    struct list_stack_S	*prev;
} list_stack_T;


typedef struct
{
    long_u	dit_todo;
    hashitem_T	*dit_hi;
} dict_iterator_T;


#define SYNSPL_DEFAULT	0	
#define SYNSPL_TOP	1	
#define SYNSPL_NOTOP	2	


#define SYNFLD_START	0	
#define SYNFLD_MINIMUM	1	


#ifdef FEAT_SPELL
# define B_SPELL(buf)  ((buf)->b_spell)
#else
# define B_SPELL(buf)  (0)
#endif

typedef struct qf_info_S qf_info_T;

#ifdef FEAT_PROFILE

typedef struct {
    proftime_T	total;		
    proftime_T	slowest;	
    long	count;		
    long	match;		
} syn_time_T;
#endif

typedef struct timer_S timer_T;
struct timer_S
{
    long	tr_id;
#ifdef FEAT_TIMERS
    timer_T	*tr_next;
    timer_T	*tr_prev;
    proftime_T	tr_due;		    
    char	tr_firing;	    
    char	tr_paused;	    
    char	tr_keep;	    
    int		tr_repeat;	    
    long	tr_interval;	    
    callback_T	tr_callback;
    int		tr_emsg_count;
#endif
};

#ifdef FEAT_CRYPT

typedef struct {
    int	    method_nr;
    void    *method_state;  
} cryptstate_T;


# define CRYPT_M_ZIP	0
# define CRYPT_M_BF	1
# define CRYPT_M_BF2	2
# define CRYPT_M_SOD    3
# define CRYPT_M_SOD2   4
# define CRYPT_M_COUNT	5 



# define CRYPT_NOT_INPLACE 1


typedef struct {
    char_u	*cat_salt;
    int		cat_salt_len;
    char_u	*cat_seed;
    int		cat_seed_len;
    char_u	*cat_add;
    int		cat_add_len;
    int		cat_init_from_file;
} crypt_arg_T;

#endif

#ifdef FEAT_PROP_POPUP
typedef enum {
    POPPOS_BOTLEFT,
    POPPOS_TOPLEFT,
    POPPOS_BOTRIGHT,
    POPPOS_TOPRIGHT,
    POPPOS_CENTER,
    POPPOS_BOTTOM,	
    POPPOS_NONE
} poppos_T;

typedef enum {
    POPCLOSE_NONE,
    POPCLOSE_BUTTON,
    POPCLOSE_CLICK
} popclose_T;

# define POPUPWIN_DEFAULT_ZINDEX	 50
# define POPUPMENU_ZINDEX		100
# define POPUPWIN_DIALOG_ZINDEX		200
# define POPUPWIN_NOTIFICATION_ZINDEX   300
#endif


typedef struct {
#ifdef FEAT_SYN_HL
    hashtab_T	b_keywtab;		
    hashtab_T	b_keywtab_ic;		
    int		b_syn_error;		
# ifdef FEAT_RELTIME
    int		b_syn_slow;		
# endif
    int		b_syn_ic;		
    int		b_syn_foldlevel;	
    int		b_syn_spell;		
    garray_T	b_syn_patterns;		
    garray_T	b_syn_clusters;		
    int		b_spell_cluster_id;	
    int		b_nospell_cluster_id;	
    int		b_syn_containedin;	
					
    int		b_syn_sync_flags;	
    short	b_syn_sync_id;		
    long	b_syn_sync_minlines;	
    long	b_syn_sync_maxlines;	
    long	b_syn_sync_linebreaks;	
    char_u	*b_syn_linecont_pat;	
    regprog_T	*b_syn_linecont_prog;	
#ifdef FEAT_PROFILE
    syn_time_T  b_syn_linecont_time;
#endif
    int		b_syn_linecont_ic;	
    int		b_syn_topgrp;		
# ifdef FEAT_CONCEAL
    int		b_syn_conceal;		
# endif
# ifdef FEAT_FOLDING
    int		b_syn_folditems;	
					
# endif
    
    synstate_T	*b_sst_array;
    int		b_sst_len;
    synstate_T	*b_sst_first;
    synstate_T	*b_sst_firstfree;
    int		b_sst_freecount;
    linenr_T	b_sst_check_lnum;
    short_u	b_sst_lasttick;	
#endif 

#ifdef FEAT_SPELL
    
    garray_T	b_langp;	    
    char_u	b_spell_ismw[256];  
    char_u	*b_spell_ismw_mb;   
    char_u	*b_p_spc;	    
    regprog_T	*b_cap_prog;	    
    char_u	*b_p_spf;	    
    char_u	*b_p_spl;	    
    char_u	*b_p_spo;	    
    int		b_cjk;		    
#endif
#if !defined(FEAT_SYN_HL) && !defined(FEAT_SPELL)
    int		dummy;
#endif
    char_u	b_syn_chartab[32];  
    char_u	*b_syn_isk;	    
} synblock_T;




struct file_buffer
{
    memline_T	b_ml;		
				

    buf_T	*b_next;	
    buf_T	*b_prev;

    int		b_nwindows;	

    int		b_flags;	
    int		b_locked;	
				
    int		b_locked_split;	
				

    
    char_u	*b_ffname;	
    char_u	*b_sfname;	
				
    char_u	*b_fname;	
				

#ifdef UNIX
    int		b_dev_valid;	
    dev_t	b_dev;		
    ino_t	b_ino;		
#endif
#ifdef VMS
    char	 b_fab_rfm;	
    char	 b_fab_rat;	
    unsigned int b_fab_mrs;	
#endif
    int		b_fnum;		
    char_u	b_key[VIM_SIZEOF_INT * 2 + 1];
				
				

    int		b_changed;	
				
    dictitem16_T b_ct_di;	
				
				
#define CHANGEDTICK(buf) ((buf)->b_ct_di.di_tv.vval.v_number)

    varnumber_T	b_last_changedtick;	
					
    varnumber_T	b_last_changedtick_pum; 
    varnumber_T	b_last_changedtick_i;   

    int		b_saving;	
				

    
    int		b_mod_set;	
				
    linenr_T	b_mod_top;	
    linenr_T	b_mod_bot;	
				
    long	b_mod_xlines;	
				

    wininfo_T	*b_wininfo;	

    long	b_mtime;	
    long	b_mtime_ns;	
    long	b_mtime_read;	
    long	b_mtime_read_ns;  
    off_T	b_orig_size;	
    int		b_orig_mode;	
#ifdef FEAT_VIMINFO
    time_T	b_last_used;	
				
#endif

    pos_T	b_namedm[NMARKS]; 

    
    visualinfo_T b_visual;
#ifdef FEAT_EVAL
    int		b_visual_mode_eval;  
#endif

    pos_T	b_last_cursor;	
				
    pos_T	b_last_insert;	
    pos_T	b_last_change;	

    
    pos_T	b_changelist[JUMPLISTSIZE];
    int		b_changelistlen;	
    int		b_new_change;		

    
    char_u	b_chartab[32];

    
    mapblock_T	*(b_maphash[256]);

    
    mapblock_T	*b_first_abbr;

    
    garray_T	b_ucmds;
    
    pos_T	b_op_start;
    pos_T	b_op_start_orig;  
    pos_T	b_op_end;

#ifdef FEAT_VIMINFO
    int		b_marks_read;	
#endif

    
    u_header_T	*b_u_oldhead;	
    u_header_T	*b_u_newhead;	
				
    u_header_T	*b_u_curhead;	
    int		b_u_numhead;	
    int		b_u_synced;	
    long	b_u_seq_last;	
    long	b_u_save_nr_last; 
    long	b_u_seq_cur;	
    time_T	b_u_time_cur;	
    long	b_u_save_nr_cur; 

    
    undoline_T	b_u_line_ptr;	
    linenr_T	b_u_line_lnum;	
    colnr_T	b_u_line_colnr;	

    int		b_scanned;	

    
    long	b_p_iminsert;	
    long	b_p_imsearch;	
#define B_IMODE_USE_INSERT (-1)	
#define B_IMODE_NONE 0		
#define B_IMODE_LMAP 1		
#define B_IMODE_IM 2		
#define B_IMODE_LAST 2

#ifdef FEAT_KEYMAP
    short	b_kmap_state;	
# define KEYMAP_INIT	1	
# define KEYMAP_LOADED	2	
    garray_T	b_kmap_ga;	
#endif

    
    int		b_p_initialized;	

#ifdef FEAT_EVAL
    sctx_T	b_p_script_ctx[BV_COUNT]; 
#endif

    int		b_p_ai;		
    int		b_p_ai_nopaste;	
    char_u	*b_p_bkc;	
    unsigned	b_bkc_flags;    
    int		b_p_ci;		
    int		b_p_bin;	
    int		b_p_bomb;	
    char_u	*b_p_bh;	
    char_u	*b_p_bt;	
#ifdef FEAT_QUICKFIX
#define BUF_HAS_QF_ENTRY 1
#define BUF_HAS_LL_ENTRY 2
    int		b_has_qf_entry;
#endif
    int		b_p_bl;		
    int		b_p_cin;	
    char_u	*b_p_cino;	
    char_u	*b_p_cink;	
    char_u	*b_p_cinsd;	
    char_u	*b_p_cinw;	
    char_u	*b_p_com;	
#ifdef FEAT_FOLDING
    char_u	*b_p_cms;	
#endif
    char_u	*b_p_cpt;	
#ifdef BACKSLASH_IN_FILENAME
    char_u	*b_p_csl;	
#endif
#ifdef FEAT_COMPL_FUNC
    char_u	*b_p_cfu;	
    callback_T	b_cfu_cb;	
    char_u	*b_p_ofu;	
    callback_T	b_ofu_cb;	
#endif
#ifdef FEAT_EVAL
    char_u	*b_p_tfu;	
    callback_T	b_tfu_cb;	
#endif
    int		b_p_eof;	
    int		b_p_eol;	
    int		b_p_fixeol;	
    int		b_p_et;		
    int		b_p_et_nobin;	
    int		b_p_et_nopaste; 
    char_u	*b_p_fenc;	
    char_u	*b_p_ff;	
    char_u	*b_p_ft;	
    char_u	*b_p_fo;	
    char_u	*b_p_flp;	
    int		b_p_inf;	
    char_u	*b_p_isk;	
#ifdef FEAT_FIND_ID
    char_u	*b_p_def;	
    char_u	*b_p_inc;	
# ifdef FEAT_EVAL
    char_u	*b_p_inex;	
    long_u	b_p_inex_flags;	
# endif
#endif
#if defined(FEAT_EVAL)
    char_u	*b_p_inde;	
    long_u	b_p_inde_flags;	
    char_u	*b_p_indk;	
#endif
    char_u	*b_p_fp;	
#if defined(FEAT_EVAL)
    char_u	*b_p_fex;	
    long_u	b_p_fex_flags;	
#endif
#ifdef FEAT_CRYPT
    char_u	*b_p_key;	
#endif
    char_u	*b_p_kp;	
    int		b_p_lisp;	
    char_u	*b_p_lop;	
    char_u	*b_p_menc;	
    char_u	*b_p_mps;	
    int		b_p_ml;		
    int		b_p_ml_nobin;	
    int		b_p_ma;		
    char_u	*b_p_nf;	
    int		b_p_pi;		
    char_u	*b_p_qe;	
    int		b_p_ro;		
    long	b_p_sw;		
    int		b_p_sn;		
    int		b_p_si;		
    long	b_p_sts;	
    long	b_p_sts_nopaste; 
    char_u	*b_p_sua;	
    int		b_p_swf;	
#ifdef FEAT_SYN_HL
    long	b_p_smc;	
    char_u	*b_p_syn;	
#endif
    long	b_p_ts;		
    int		b_p_tx;		
    long	b_p_tw;		
    long	b_p_tw_nobin;	
    long	b_p_tw_nopaste;	
    long	b_p_wm;		
    long	b_p_wm_nobin;	
    long	b_p_wm_nopaste;	
#ifdef FEAT_VARTABS
    char_u	*b_p_vsts;	
    int		*b_p_vsts_array;   
    char_u	*b_p_vsts_nopaste; 
    char_u	*b_p_vts;	
    int		*b_p_vts_array;	
#endif
#ifdef FEAT_KEYMAP
    char_u	*b_p_keymap;	
#endif

    
#ifdef FEAT_QUICKFIX
    char_u	*b_p_gp;	
    char_u	*b_p_mp;	
    char_u	*b_p_efm;	
#endif
    char_u	*b_p_ep;	
    char_u	*b_p_path;	
    int		b_p_ar;		
    char_u	*b_p_tags;	
    char_u	*b_p_tc;	
    unsigned	b_tc_flags;     
    char_u	*b_p_dict;	
    char_u	*b_p_tsr;	
#ifdef FEAT_COMPL_FUNC
    char_u	*b_p_tsrfu;	
    callback_T	b_tsrfu_cb;	
#endif
    long	b_p_ul;		
#ifdef FEAT_PERSISTENT_UNDO
    int		b_p_udf;	
#endif
    char_u	*b_p_lw;	
#ifdef FEAT_TERMINAL
    long	b_p_twsl;	
#endif

    

    
    int		b_ind_level;
    int		b_ind_open_imag;
    int		b_ind_no_brace;
    int		b_ind_first_open;
    int		b_ind_open_extra;
    int		b_ind_close_extra;
    int		b_ind_open_left_imag;
    int		b_ind_jump_label;
    int		b_ind_case;
    int		b_ind_case_code;
    int		b_ind_case_break;
    int		b_ind_param;
    int		b_ind_func_type;
    int		b_ind_comment;
    int		b_ind_in_comment;
    int		b_ind_in_comment2;
    int		b_ind_cpp_baseclass;
    int		b_ind_continuation;
    int		b_ind_unclosed;
    int		b_ind_unclosed2;
    int		b_ind_unclosed_noignore;
    int		b_ind_unclosed_wrapped;
    int		b_ind_unclosed_whiteok;
    int		b_ind_matching_paren;
    int		b_ind_paren_prev;
    int		b_ind_maxparen;
    int		b_ind_maxcomment;
    int		b_ind_scopedecl;
    int		b_ind_scopedecl_code;
    int		b_ind_java;
    int		b_ind_js;
    int		b_ind_keep_case_label;
    int		b_ind_hash_comment;
    int		b_ind_cpp_namespace;
    int		b_ind_if_for_while;
    int		b_ind_cpp_extern_c;
    int		b_ind_pragma;

    linenr_T	b_no_eol_lnum;	
				

    int		b_start_eof;	
    int		b_start_eol;	
    int		b_start_ffc;	
    char_u	*b_start_fenc;	
    int		b_bad_char;	
    int		b_start_bomb;	

#ifdef FEAT_EVAL
    dictitem_T	b_bufvar;	
    dict_T	*b_vars;	

    listener_T	*b_listener;
    list_T	*b_recorded_changes;
#endif
#ifdef FEAT_PROP_POPUP
    int		b_has_textprop;	
    hashtab_T	*b_proptypes;	
    proptype_T	**b_proparray;	
    garray_T	b_textprop_text; 
#endif

#if defined(FEAT_BEVAL) && defined(FEAT_EVAL)
    char_u	*b_p_bexpr;	
    long_u	b_p_bexpr_flags;
#endif
#ifdef FEAT_CRYPT
    char_u	*b_p_cm;	
#endif

    
    
    
    int		b_may_swap;
    int		b_did_warn;	
				

    
    
    
    
    int		b_help;		
				
#ifdef FEAT_SPELL
    int		b_spell;	
				
				
#endif

    int		b_shortname;	

#ifdef FEAT_JOB_CHANNEL
    char_u	*b_prompt_text;		
    callback_T	b_prompt_callback;	
    callback_T	b_prompt_interrupt;	
    int		b_prompt_insert;	
					
#endif
#ifdef FEAT_MZSCHEME
    void	*b_mzscheme_ref; 
#endif

#ifdef FEAT_PERL
    void	*b_perl_private;
#endif

#ifdef FEAT_PYTHON
    void	*b_python_ref;	
#endif

#ifdef FEAT_PYTHON3
    void	*b_python3_ref;	
#endif

#ifdef FEAT_TCL
    void	*b_tcl_ref;
#endif

#ifdef FEAT_RUBY
    void	*b_ruby_ref;
#endif

#if defined(FEAT_SYN_HL) || defined(FEAT_SPELL)
    synblock_T	b_s;		
				
				
#endif

#ifdef FEAT_SIGNS
    sign_entry_T *b_signlist;	   
# ifdef FEAT_NETBEANS_INTG
    int		b_has_sign_column; 
				   
				   
# endif
#endif

#ifdef FEAT_NETBEANS_INTG
    int		b_netbeans_file;    
    int		b_was_netbeans_file;
#endif
#ifdef FEAT_JOB_CHANNEL
    int		b_write_to_channel; 
				    
#endif

#ifdef FEAT_CRYPT
    cryptstate_T *b_cryptstate;	
				
#endif
    int		b_mapped_ctrl_c; 

#ifdef FEAT_TERMINAL
    term_T	*b_term;	
				
#endif
#ifdef FEAT_DIFF
    int		b_diff_failed;	
#endif
}; 


#ifdef FEAT_DIFF

# define DB_COUNT 8	


typedef struct diffblock_S diff_T;
struct diffblock_S
{
    diff_T	*df_next;
    linenr_T	df_lnum[DB_COUNT];	
    linenr_T	df_count[DB_COUNT];	
};
#endif

#define SNAP_HELP_IDX	0
#define SNAP_AUCMD_IDX	1
#define SNAP_COUNT	2


typedef struct tabpage_S tabpage_T;
struct tabpage_S
{
    tabpage_T	    *tp_next;	    
    frame_T	    *tp_topframe;   
    win_T	    *tp_curwin;	    
    win_T	    *tp_prevwin;    
    win_T	    *tp_firstwin;   
    win_T	    *tp_lastwin;    
#ifdef FEAT_PROP_POPUP
    win_T	    *tp_first_popupwin; 
#endif
    long	    tp_old_Rows;    
    long	    tp_old_Columns; 
				    
    long	    tp_ch_used;	    
				    
#ifdef FEAT_GUI
    int		    tp_prev_which_scrollbars[3];
				    
#endif

    char_u	    *tp_localdir;	
					
    char_u	    *tp_prevdir;	

#ifdef FEAT_DIFF
    diff_T	    *tp_first_diff;
    buf_T	    *(tp_diffbuf[DB_COUNT]);
    int		    tp_diff_invalid;	
    int		    tp_diff_update;	
#endif
    frame_T	    *(tp_snapshot[SNAP_COUNT]);  
#ifdef FEAT_EVAL
    dictitem_T	    tp_winvar;	    
    dict_T	    *tp_vars;	    
#endif

#ifdef FEAT_PYTHON
    void	    *tp_python_ref;	
#endif

#ifdef FEAT_PYTHON3
    void	    *tp_python3_ref;	
#endif
};


typedef struct w_line
{
    linenr_T	wl_lnum;	
    short_u	wl_size;	
    char	wl_valid;	
#ifdef FEAT_FOLDING
    char	wl_folded;	
    linenr_T	wl_lastlnum;	
#endif
} wline_T;


struct frame_S
{
    char	fr_layout;	
    int		fr_width;
    int		fr_newwidth;	
    int		fr_height;
    int		fr_newheight;	
    frame_T	*fr_parent;	
    frame_T	*fr_next;	
				
    frame_T	*fr_prev;	
				
    
    frame_T	*fr_child;	
    win_T	*fr_win;	
				
};

#define FR_LEAF	0	
#define FR_ROW	1	
#define FR_COL	2	


typedef struct
{
    regmmatch_T	rm;	    
			    
    buf_T	*buf;	    
    linenr_T	lnum;	    
    int		attr;	    
    int		attr_cur;   
    linenr_T	first_lnum; 
    colnr_T	startcol;   
    colnr_T	endcol;	    
    char	is_addpos;  
			    
    char	has_cursor; 
			    
} match_T;


typedef struct
{
    linenr_T	lnum;	
    colnr_T	col;	
    int		len;	
} llpos_T;


typedef struct matchitem matchitem_T;
struct matchitem
{
    matchitem_T	*mit_next;
    int		mit_id;		
    int		mit_priority;   

    
    
    char_u	*mit_pattern;   
    regmmatch_T	mit_match;	

    llpos_T	*mit_pos_array;	
    int		mit_pos_count;	
    int		mit_pos_cur;	
    linenr_T	mit_toplnum;	
    linenr_T	mit_botlnum;	

    match_T	mit_hl;		
    int		mit_hlg_id;	
#ifdef FEAT_CONCEAL
    int		mit_conceal_char; 
#endif
};



typedef struct
{
    int		w_topline_save;	
    int		w_topline_corr;	
    pos_T	w_cursor_save;	
    pos_T	w_cursor_corr;	
} pos_save_T;

#ifdef FEAT_MENU
typedef struct {
    int		wb_startcol;
    int		wb_endcol;
    vimmenu_T	*wb_menu;
} winbar_item_T;
#endif


typedef struct
{
    int		eol;
    int		ext;
    int		prec;
    int		nbsp;
    int		space;
    int		tab1;
    int		tab2;
    int		tab3;
    int		trail;
    int		lead;
    int		*multispace;
    int		*leadmultispace;
#ifdef FEAT_CONCEAL
    int		conceal;
#endif
} lcs_chars_T;


typedef struct
{
    int	stl;
    int	stlnc;
    int	vert;
    int	fold;
    int	foldopen;
    int	foldclosed;
    int	foldsep;
    int	diff;
    int	eob;
    int	lastline;
} fill_chars_T;


struct window_S
{
    int		w_id;		    

    buf_T	*w_buffer;	    

    win_T	*w_prev;	    
    win_T	*w_next;	    

#if defined(FEAT_SYN_HL) || defined(FEAT_SPELL)
    synblock_T	*w_s;		    
#endif

    int		w_closing;	    
				    

    frame_T	*w_frame;	    

    pos_T	w_cursor;	    

    colnr_T	w_curswant;	    
				    
				    

    int		w_set_curswant;	    
				    
				    

#ifdef FEAT_SYN_HL
    linenr_T	w_last_cursorline;  
#endif

    
    char	w_old_visual_mode;  
    linenr_T	w_old_cursor_lnum;  
    colnr_T	w_old_cursor_fcol;  
    colnr_T	w_old_cursor_lcol;  
    linenr_T	w_old_visual_lnum;  
    colnr_T	w_old_visual_col;   
    colnr_T	w_old_curswant;	    

    linenr_T    w_last_cursor_lnum_rnu;  
					 

    lcs_chars_T	w_lcs_chars;	    
    fill_chars_T w_fill_chars;	    

    
    linenr_T	w_topline;	    
				    
    char	w_topline_was_set;  
				    

    linenr_T	w_botline;	    
				    

#ifdef FEAT_DIFF
    int		w_topfill;	    
    int		w_old_topfill;	    
    int		w_botfill;	    
				    
    int		w_old_botfill;	    
#endif
    colnr_T	w_leftcol;	    
				    
				    
    colnr_T	w_skipcol;	    
				    
				    

    int		w_empty_rows;	    
#ifdef FEAT_DIFF
    int		w_filler_rows;	    
				    
#endif

    
    linenr_T	w_last_topline;	    
#ifdef FEAT_DIFF
    int		w_last_topfill;	    
#endif
    colnr_T	w_last_leftcol;	    
    colnr_T	w_last_skipcol;	    
    int		w_last_width;	    
    int		w_last_height;	    

    
    int		w_winrow;	    
    int		w_height;	    
				    
    int		w_prev_winrow;	    
    int		w_prev_height;	    

    int		w_status_height;    
    int		w_wincol;	    
    int		w_width;	    
    int		w_vsep_width;	    

    pos_save_T	w_save_cursor;	    
    int		w_do_win_fix_cursor;

#ifdef FEAT_PROP_POPUP
    int		w_popup_flags;	    
    int		w_popup_handled;    
    char_u	*w_popup_title;
    poppos_T	w_popup_pos;
    int		w_popup_fixed;	    
    int		w_popup_prop_type;  
    win_T	*w_popup_prop_win;  
    int		w_popup_prop_id;    
    int		w_zindex;
    int		w_minheight;	    
    int		w_minwidth;	    
    int		w_maxheight;	    
    int		w_maxwidth;	    
    int		w_maxwidth_opt;	    
    int		w_wantline;	    
    int		w_wantcol;	    
    int		w_firstline;	    
    int		w_want_scrollbar;   
    int		w_has_scrollbar;    
    char_u	*w_scrollbar_highlight; 
    char_u	*w_thumb_highlight; 
    int		w_popup_padding[4]; 
    int		w_popup_border[4];  
    char_u	*w_border_highlight[4];  
    int		w_border_char[8];   

    int		w_popup_leftoff;    
    int		w_popup_rightoff;   
    varnumber_T	w_popup_last_changedtick; 
					  
    varnumber_T	w_popup_prop_changedtick; 
					  
					  
    int		w_popup_prop_topline; 
				      
				      
    linenr_T	w_popup_last_curline; 
				      
    callback_T	w_close_cb;	    
    callback_T	w_filter_cb;	    
    int		w_filter_errors;    
    int		w_filter_mode;	    

    win_T	*w_popup_curwin;    
    linenr_T	w_popup_lnum;	    
    colnr_T	w_popup_mincol;	    
    colnr_T	w_popup_maxcol;	    
    int		w_popup_mouse_row;  
    int		w_popup_mouse_mincol;  
    int		w_popup_mouse_maxcol;  
    popclose_T	w_popup_close;	    

    list_T	*w_popup_mask;	     
    char_u	*w_popup_mask_cells; 
    int		w_popup_mask_height; 
    int		w_popup_mask_width;  
# if defined(FEAT_TIMERS)
    timer_T	*w_popup_timer;	    
# endif

    int		w_flags;	    

# define WFLAG_WCOL_OFF_ADDED	1   
				    
# define WFLAG_WROW_OFF_ADDED	2   
				    
#endif

    
    
    int		w_valid;
    pos_T	w_valid_cursor;	    
				    
    colnr_T	w_valid_leftcol;    
    colnr_T	w_valid_skipcol;    

    
    int		w_cline_height;	    
#ifdef FEAT_FOLDING
    int		w_cline_folded;	    
#endif

    int		w_cline_row;	    

    colnr_T	w_virtcol;	    
				    
				    
				    
				    
				    

#ifdef FEAT_PROP_POPUP
    colnr_T	w_virtcol_first_char;	
					
					
#endif
    
    int		w_wrow, w_wcol;	    

    
    int		w_lines_valid;	    
    wline_T	*w_lines;

#ifdef FEAT_FOLDING
    garray_T	w_folds;	    
    char	w_fold_manual;	    
				    
    char	w_foldinvalid;	    
				    
#endif
#ifdef FEAT_LINEBREAK
    int		w_nrwidth;	    
				    
#endif
#ifdef FEAT_TERMINAL
    termcellcolor_T w_term_wincolor;	 
#endif

    

    int		w_redr_type;	    
    int		w_upd_rows;	    
				    
    linenr_T	w_redraw_top;	    
    linenr_T	w_redraw_bot;	    
    int		w_redr_status;	    

    
    pos_T	w_ru_cursor;	    
    colnr_T	w_ru_virtcol;	    
    linenr_T	w_ru_topline;	    
    linenr_T	w_ru_line_count;    
#ifdef FEAT_DIFF
    int		w_ru_topfill;	    
#endif
    char	w_ru_empty;	    

    int		w_alt_fnum;	    

    alist_T	*w_alist;	    
    int		w_arg_idx;	    
				    
    int		w_arg_idx_invalid;  

    char_u	*w_localdir;	    
				    
    char_u	*w_prevdir;	    
#ifdef FEAT_MENU
    vimmenu_T	*w_winbar;	    
    winbar_item_T *w_winbar_items;  
    int		w_winbar_height;    
#endif

    
    winopt_T	w_onebuf_opt;
    winopt_T	w_allbuf_opt;
    
#define GLOBAL_WO(p)	((char *)(p) + sizeof(winopt_T))

    
#ifdef FEAT_STL_OPT
    long_u	w_p_stl_flags;	    
#endif
#ifdef FEAT_EVAL
    long_u	w_p_fde_flags;	    
    long_u	w_p_fdt_flags;	    
#endif
#if defined(FEAT_SIGNS) || defined(FEAT_FOLDING) || defined(FEAT_DIFF)
    int		*w_p_cc_cols;	    
    char_u	w_p_culopt_flags;   
#endif

#ifdef FEAT_LINEBREAK
    int		w_briopt_min;	    
    int		w_briopt_shift;	    
    int		w_briopt_sbr;	    
    int		w_briopt_list;      
    int		w_briopt_vcol;	    
#endif

    long	w_scbind_pos;

#ifdef FEAT_EVAL
    dictitem_T	w_winvar;	
    dict_T	*w_vars;	
#endif

    
    pos_T	w_pcmark;	
    pos_T	w_prev_pcmark;	

    
    xfmark_T	w_jumplist[JUMPLISTSIZE];
    int		w_jumplistlen;		
    int		w_jumplistidx;		

    int		w_changelistidx;	

#ifdef FEAT_SEARCH_EXTRA
    matchitem_T	*w_match_head;		
    int		w_next_match_id;	
#endif

    
    taggy_T	w_tagstack[TAGSTACKSIZE];   
    int		w_tagstackidx;		    
    int		w_tagstacklen;		    

    
    int		w_fraction;
    int		w_prev_fraction_row;

#ifdef FEAT_GUI
    scrollbar_T	w_scrollbars[2];	
#endif
#ifdef FEAT_LINEBREAK
    linenr_T	w_nrwidth_line_count;	
					
    long	w_nuw_cached;		
    int		w_nrwidth_width;	
#endif

#ifdef FEAT_QUICKFIX
    qf_info_T	*w_llist;		
    
    qf_info_T	*w_llist_ref;
#endif

#ifdef FEAT_MZSCHEME
    void	*w_mzscheme_ref;	
#endif

#ifdef FEAT_PERL
    void	*w_perl_private;
#endif

#ifdef FEAT_PYTHON
    void	*w_python_ref;		
#endif

#ifdef FEAT_PYTHON3
    void	*w_python3_ref;		
#endif

#ifdef FEAT_TCL
    void	*w_tcl_ref;
#endif

#ifdef FEAT_RUBY
    void	*w_ruby_ref;
#endif
};


typedef struct oparg_S
{
    int		op_type;	
    int		regname;	
    int		motion_type;	
    int		motion_force;	
    int		use_reg_one;	
				
    int		inclusive;	
				
    int		end_adjusted;	
				
    pos_T	start;		
    pos_T	end;		
    pos_T	cursor_start;	

    long	line_count;	
				
    int		empty;		
				
    int		is_VIsual;	
    int		block_mode;	
    colnr_T	start_vcol;	
    colnr_T	end_vcol;	
    long	prev_opcount;	
    long	prev_count0;	
    int		excl_tr_ws;	
				
} oparg_T;


typedef struct cmdarg_S
{
    oparg_T	*oap;		
    int		prechar;	
    int		cmdchar;	
    int		nchar;		
    int		ncharC1;	
    int		ncharC2;	
    int		extra_char;	
    long	opcount;	
    long	count0;		
    long	count1;		
    int		arg;		
    int		retval;		
    char_u	*searchbuf;	
} cmdarg_T;


#define CA_COMMAND_BUSY	    1	
#define CA_NO_ADJ_OP_END    2	

#ifdef CURSOR_SHAPE


#define SHAPE_IDX_N	0	
#define SHAPE_IDX_V	1	
#define SHAPE_IDX_I	2	
#define SHAPE_IDX_R	3	
#define SHAPE_IDX_C	4	
#define SHAPE_IDX_CI	5	
#define SHAPE_IDX_CR	6	
#define SHAPE_IDX_O	7	
#define SHAPE_IDX_VE	8	
#define SHAPE_IDX_CLINE	9	
#define SHAPE_IDX_STATUS 10	
#define SHAPE_IDX_SDRAG 11	
#define SHAPE_IDX_VSEP	12	
#define SHAPE_IDX_VDRAG 13	
#define SHAPE_IDX_MORE	14	
#define SHAPE_IDX_MOREL	15	
#define SHAPE_IDX_SM	16	
#define SHAPE_IDX_COUNT	17

#define SHAPE_BLOCK	0	
#define SHAPE_HOR	1	
#define SHAPE_VER	2	

#define MSHAPE_NUMBERED	1000	
#define MSHAPE_HIDE	1	

#define SHAPE_MOUSE	1	
#define SHAPE_CURSOR	2	

typedef struct cursor_entry
{
    int		shape;		
    int		mshape;		
    int		percentage;	
    long	blinkwait;	
    long	blinkon;	
    long	blinkoff;	
    int		id;		
    int		id_lm;		
    char	*name;		
    char	used_for;	
} cursorentry_T;
#endif 

#ifdef FEAT_MENU


#define MENU_INDEX_INVALID	-1
#define MENU_INDEX_NORMAL	0
#define MENU_INDEX_VISUAL	1
#define MENU_INDEX_SELECT	2
#define MENU_INDEX_OP_PENDING	3
#define MENU_INDEX_INSERT	4
#define MENU_INDEX_CMDLINE	5
#define MENU_INDEX_TERMINAL	6
#define MENU_INDEX_TIP		7
#define MENU_MODES		8


#define MENU_NORMAL_MODE	(1 << MENU_INDEX_NORMAL)
#define MENU_VISUAL_MODE	(1 << MENU_INDEX_VISUAL)
#define MENU_SELECT_MODE	(1 << MENU_INDEX_SELECT)
#define MENU_OP_PENDING_MODE	(1 << MENU_INDEX_OP_PENDING)
#define MENU_INSERT_MODE	(1 << MENU_INDEX_INSERT)
#define MENU_CMDLINE_MODE	(1 << MENU_INDEX_CMDLINE)
#define MENU_TERMINAL_MODE	(1 << MENU_INDEX_TERMINAL)
#define MENU_TIP_MODE		(1 << MENU_INDEX_TIP)
#define MENU_ALL_MODES		((1 << MENU_INDEX_TIP) - 1)



#define MNU_HIDDEN_CHAR		']'

struct VimMenu
{
    int		modes;		    
    int		enabled;	    
    char_u	*name;		    
    char_u	*dname;		    
#ifdef FEAT_MULTI_LANG
    char_u	*en_name;	    
				    
    char_u	*en_dname;	    
				    
#endif
    char_u	*actext;	    
    int		mnemonic;	    
    int		priority;	    
#ifdef FEAT_GUI
    void	(*cb)(vimmenu_T *); 
#endif
#ifdef FEAT_TOOLBAR
    char_u	*iconfile;	    
    int		iconidx;	    
    int		icon_builtin;	    
#endif
    char_u	*strings[MENU_MODES]; 
    int		noremap[MENU_MODES]; 
    char	silent[MENU_MODES]; 
    vimmenu_T	*children;	    
    vimmenu_T	*parent;	    
    vimmenu_T	*next;		    
#ifdef FEAT_GUI_X11
    Widget	id;		    
    Widget	submenu_id;	    
#endif
#ifdef FEAT_GUI_GTK
    GtkWidget	*id;		    
    GtkWidget	*submenu_id;	    
# if defined(GTK_CHECK_VERSION) && !GTK_CHECK_VERSION(3,4,0)
    GtkWidget	*tearoff_handle;
# endif
    GtkWidget   *label;		    
#endif
#ifdef FEAT_GUI_MOTIF
    int		sensitive;	    
    char	**xpm;		    
    char	*xpm_fname;	    
#endif
#ifdef FEAT_BEVAL_TIP
    BalloonEval *tip;		    
#endif
#ifdef FEAT_GUI_MSWIN
    UINT	id;		    
    HMENU	submenu_id;	    
    HWND	tearoff_handle;	    
#endif
#if FEAT_GUI_HAIKU
    BMenuItem  *id;		    
    BMenu  *submenu_id;		    
# ifdef FEAT_TOOLBAR
    BPictureButton *button;
# endif
#endif
#ifdef FEAT_GUI_PHOTON
    PtWidget_t	*id;
    PtWidget_t	*submenu_id;
#endif
};
#else

typedef int vimmenu_T;

#endif 


typedef struct
{
    buf_T	*save_curbuf;	    
    int		use_aucmd_win_idx;  
    int		save_curwin_id;	    
    int		new_curwin_id;	    
    int		save_prevwin_id;    
    bufref_T	new_curbuf;	    
    char_u	*globaldir;	    
    int		save_VIsual_active; 
    int		save_State;	    
} aco_save_T;


typedef struct
{
    const char	*name;
    int		hasnum;
    long	number;
    char_u	*string;	
    int		strlen;
    int		present;
} option_table_T;


typedef struct
{
    long_u	fg_color;
    long_u	bg_color;
    int		bold;
    int		italic;
    int		underline;
    int		undercurl;
} prt_text_attr_T;


typedef struct
{
    int		n_collated_copies;
    int		n_uncollated_copies;
    int		duplex;
    int		chars_per_line;
    int		lines_per_page;
    int		has_color;
    prt_text_attr_T number;
#ifdef FEAT_SYN_HL
    int		modec;
    int		do_syntax;
#endif
    int		user_abort;
    char_u	*jobname;
#ifdef FEAT_POSTSCRIPT
    char_u	*outfile;
    char_u	*arguments;
#endif
} prt_settings_T;

#define PRINT_NUMBER_WIDTH 8


typedef struct
{
    char_u	*pum_text;	
    char_u	*pum_kind;	
    char_u	*pum_extra;	
    char_u	*pum_info;	
} pumitem_T;


typedef struct
{
    char_u	*tn_tags;	
    char_u	*tn_np;		
    int		tn_did_filefind_init;
    int		tn_hf_idx;
    void	*tn_search_ctx;
} tagname_T;

typedef struct {
  UINT32_T total[2];
  UINT32_T state[8];
  char_u   buffer[64];
} context_sha256_T;


typedef enum
{
    EXPR_UNKNOWN = 0,
    EXPR_EQUAL,		
    EXPR_NEQUAL,	
    EXPR_GREATER,	
    EXPR_GEQUAL,	
    EXPR_SMALLER,	
    EXPR_SEQUAL,	
    EXPR_MATCH,		
    EXPR_NOMATCH,	
    EXPR_IS,		
    EXPR_ISNOT,		
    
    EXPR_ADD,		
    EXPR_SUB,		
    EXPR_MULT,		
    EXPR_DIV,		
    EXPR_REM,		
    EXPR_LSHIFT,	
    EXPR_RSHIFT,	
    
    EXPR_COPY,		
    EXPR_APPEND,	
} exprtype_T;


struct js_reader
{
    char_u	*js_buf;	
    char_u	*js_end;	
    int		js_used;	
    int		(*js_fill)(struct js_reader *);
				
				
    void	*js_cookie;	
    int		js_cookie_arg;	
};
typedef struct js_reader js_read_T;


#define MAX_ARG_CMDS 10


#define WIN_HOR	    1	    
#define	WIN_VER	    2	    
#define	WIN_TABS    3	    


typedef struct
{
    int		argc;
    char	**argv;

    char_u	*fname;			

    int		evim_mode;		
    char_u	*use_vimrc;		
    int		clean;			

    int		n_commands;		     
    char_u	*commands[MAX_ARG_CMDS];     
    char_u	cmds_tofree[MAX_ARG_CMDS];   
    int		n_pre_commands;		     
    char_u	*pre_commands[MAX_ARG_CMDS]; 

    int		edit_type;		
    char_u	*tagname;		
#ifdef FEAT_QUICKFIX
    char_u	*use_ef;		
#endif

    int		want_full_screen;
    int		not_a_term;		
#ifdef FEAT_GUI
    char_u	*gui_dialog_file;	
#endif
    int		tty_fail;		
    char_u	*term;			
#ifdef FEAT_CRYPT
    int		ask_for_key;		
#endif
    int		no_swap_file;		
#ifdef FEAT_EVAL
    int		use_debug_break_level;
#endif
    int		window_count;		
    int		window_layout;		

#ifdef FEAT_CLIENTSERVER
    int		serverArg;		
    char_u	*serverName_arg;	
    char_u	*serverStr;		
    char_u	*serverStrEnc;		
    char_u	*servername;		
#endif
#if !defined(UNIX)
# define EXPAND_FILENAMES
    int		literal;		
#endif
#ifdef MSWIN
    int		full_path;		
#endif
#ifdef FEAT_DIFF
    int		diff_mode;		
#endif
} mparm_T;


typedef struct lval_S
{
    char_u	*ll_name;	
    char_u	*ll_name_end;	
    type_T	*ll_type;	
    char_u	*ll_exp_name;	

    scid_T	ll_sid;		
				

    typval_T	*ll_tv;		
				
				
    listitem_T	*ll_li;		
    list_T	*ll_list;	
    int		ll_range;	
    int		ll_empty2;	
    long	ll_n1;		
    long	ll_n2;		
    dict_T	*ll_dict;	
    dictitem_T	*ll_di;		
    char_u	*ll_newkey;	
    type_T	*ll_valtype;	
    blob_T	*ll_blob;	
    ufunc_T	*ll_ufunc;	
    object_T	*ll_object;	
    class_T	*ll_class;	
    int		ll_oi;		
    int		ll_is_root;	
				
} lval_T;


typedef struct lval_root_S {
    typval_T	*lr_tv;		
    class_T	*lr_cl_exec;	
    int		lr_is_arg;	
} lval_root_T;



typedef struct {
    int		save_msg_scroll;
    int		save_restart_edit;
    int		save_msg_didout;
    int		save_State;
    int		save_insertmode;
    int		save_finish_op;
    int		save_opcount;
    int		save_reg_executing;
    int		save_pending_end_reg_executing;
    int		save_script_version;
    tasave_T	tabuf;
} save_state_T;

typedef struct {
    varnumber_T vv_prevcount;
    varnumber_T vv_count;
    varnumber_T vv_count1;
} vimvars_save_T;


typedef enum {
    CDSCOPE_GLOBAL,	
    CDSCOPE_TABPAGE,	
    CDSCOPE_WINDOW	
} cdscope_T;


typedef enum
{
    VAR_FLAVOUR_DEFAULT,	
    VAR_FLAVOUR_SESSION,	
    VAR_FLAVOUR_VIMINFO		
} var_flavour_T;


typedef enum {
    IGNORE_POPUP,	
    FIND_POPUP,		
    FAIL_POPUP		
} mouse_find_T;


#define DELETION_REGISTER	36
#ifdef FEAT_CLIPBOARD
# define STAR_REGISTER		37
#  ifdef FEAT_X11
#   define PLUS_REGISTER	38
#  else
#   define PLUS_REGISTER	STAR_REGISTER	    
#  endif
#endif
#ifdef FEAT_DND
# define TILDE_REGISTER		(PLUS_REGISTER + 1)
#endif

#ifdef FEAT_CLIPBOARD
# ifdef FEAT_DND
#  define NUM_REGISTERS		(TILDE_REGISTER + 1)
# else
#  define NUM_REGISTERS		(PLUS_REGISTER + 1)
# endif
#else
# define NUM_REGISTERS		37
#endif



struct block_def
{
    int		startspaces;	
    int		endspaces;	
    int		textlen;	
    char_u	*textstart;	
    colnr_T	textcol;	
    colnr_T	start_vcol;	
    colnr_T	end_vcol;	
    int		is_short;	
    int		is_MAX;		
    int		is_oneChar;	
    int		pre_whitesp;	
    int		pre_whitesp_c;	
    colnr_T	end_char_vcols;	
    colnr_T	start_char_vcols; 
};


typedef struct
{
    char_u	**y_array;	
    linenr_T	y_size;		
    char_u	y_type;		
    colnr_T	y_width;	
#ifdef FEAT_VIMINFO
    time_t	y_time_set;
#endif
} yankreg_T;



typedef struct soffset
{
    int		dir;		
    int		line;		
    int		end;		
    long	off;		
} soffset_T;


typedef struct spat
{
    char_u	    *pat;	
    int		    magic;	
    int		    no_scs;	
    soffset_T	    off;
} spat_T;


typedef struct
{
    linenr_T	sa_stop_lnum;	
#ifdef FEAT_RELTIME
    long	sa_tm;		
    int		sa_timed_out;	
#endif
    int		sa_wrapped;	
} searchit_arg_T;



typedef struct {
    FILE	*fp;		
    char_u	*nextline;	
    linenr_T	sourcing_lnum;	
    int		finished;	
    int		source_from_buf;
    int		buf_lnum;	
    garray_T	buflines;	
#ifdef USE_CRNL
    int		fileformat;	
    int		error;		
#endif
#ifdef FEAT_EVAL
    linenr_T	breakpoint;	
    char_u	*fname;		
    int		dbg_tick;	
    int		level;		
#endif
    vimconv_T	conv;		
} source_cookie_T;


#define WRITEBUFSIZE	8192	

#define FIO_LATIN1	0x01	
#define FIO_UTF8	0x02	
#define FIO_UCS2	0x04	
#define FIO_UCS4	0x08	
#define FIO_UTF16	0x10	
#ifdef MSWIN
# define FIO_CODEPAGE	0x20	
# define FIO_PUT_CP(x) (((x) & 0xffff) << 16)	
# define FIO_GET_CP(x)	(((x)>>16) & 0xffff)	
#endif
#ifdef MACOS_CONVERT
# define FIO_MACROMAN	0x20	
#endif
#define FIO_ENDIAN_L	0x80	
#define FIO_ENCRYPTED	0x1000	
#define FIO_NOCONVERT	0x2000	
#define FIO_UCSBOM	0x4000	
#define FIO_ALL	(-1)	



#define CONV_RESTLEN 30



#define ICONV_MULT 8


typedef enum {
    OPTION_MAGIC_NOT_SET,	
    OPTION_MAGIC_ON,		
    OPTION_MAGIC_OFF		
} optmagic_T;





typedef enum {
    MAGIC_NONE = 1,		
    MAGIC_OFF = 2,		
    MAGIC_ON = 3,		
    MAGIC_ALL = 4		
} magic_T;

typedef enum {
    WT_UNKNOWN = 0,	
    WT_ARGUMENT,
    WT_VARIABLE,
    WT_MEMBER,
    WT_METHOD,		
    WT_METHOD_ARG,	
    WT_METHOD_RETURN	
} wherekind_T;





typedef struct {
    char	*wt_func_name;  
    char	wt_index;	
    wherekind_T	wt_kind;	
} where_T;

#define WHERE_INIT {NULL, 0, WT_UNKNOWN}


typedef struct {
    int		sve_did_save;
    hashtab_T	sve_hashtab;
} save_v_event_T;


typedef enum {
    FILTERMAP_FILTER,
    FILTERMAP_MAP,
    FILTERMAP_MAPNEW
} filtermap_T;


typedef struct {
    win_T	*sw_curwin;
    tabpage_T	*sw_curtab;
    int		sw_same_win;	    
    int		sw_visual_active;
} switchwin_T;



typedef struct {
    int		idx;
    char_u	*str;
    int		score;
} fuzmatch_str_T;


typedef struct {
    win_T	*cts_win;
    char_u	*cts_line;		
    char_u	*cts_ptr;		
#ifdef FEAT_PROP_POPUP
    int		cts_text_prop_count;	
					
    textprop_T	*cts_text_props;	
    char	cts_has_prop_with_text;	
    int		cts_cur_text_width;	
    int		cts_prop_lines;		
    int		cts_first_char;		
    int		cts_with_trailing;	
					
    int		cts_start_incl;		
#endif
    int		cts_vcol;		
    int		cts_max_head_vcol;	
} chartabsize_T;


typedef struct
{
    
    
    char_u	*os_varp;
    int		os_idx;
    int		os_flags;
    set_op_T	os_op;

    
    union
    {
	long	number;
	int	boolean;
	char_u	*string;
    } os_oldval;

    
    union
    {
	long	number;
	int	boolean;
	char_u	*string;
    } os_newval;

    
    
    int		os_value_checked;
    
    int		os_value_changed;

    
    
    
    int		os_restore_chartab;

#if defined(FEAT_VTP) && defined(FEAT_TERMGUICOLORS)
    
    int		os_did_swaptcap;
#endif

    
    
    
    char	*os_errbuf;
    
    size_t	os_errbuflen;
} optset_T;


typedef struct
{
    
    char_u	*oe_varp;
    
    char_u	*oe_opt_value;

    
    int		oe_append;
    
    
    int		oe_include_orig_val;

    
    regmatch_T	*oe_regmatch;
    
    expand_T	*oe_xp;

    
    
    
    char_u	*oe_set_arg;
} optexpand_T;


typedef struct {
    int		spv_has_spell;	    
#ifdef FEAT_SPELL
    int		spv_unchanged;	    
    int		spv_checked_col;    
				    
    linenr_T	spv_checked_lnum;   
    int		spv_cap_col;	    
    linenr_T	spv_capcol_lnum;    
#endif
} spellvars_T;
