



#ifdef VMS
# include <float.h>
#endif

typedef enum {
    ISN_EXEC,	    
    ISN_EXECCONCAT, 
    ISN_EXEC_SPLIT, 
    ISN_EXECRANGE,  
    ISN_LEGACY_EVAL, 
    ISN_ECHO,	    
    ISN_EXECUTE,    
    ISN_ECHOMSG,    
    ISN_ECHOCONSOLE, 
    ISN_ECHOWINDOW, 
    ISN_ECHOERR,    
    ISN_RANGE,	    
    ISN_SUBSTITUTE, 

    ISN_SOURCE,	    
    ISN_INSTR,	    
    ISN_CONSTRUCT,  
    ISN_GET_OBJ_MEMBER, 
    ISN_GET_ITF_MEMBER, 
    ISN_STORE_THIS, 
		    
    ISN_LOAD_CLASSMEMBER,  
    ISN_STORE_CLASSMEMBER,  

    
    ISN_LOAD,	    
    ISN_LOADV,	    
    ISN_LOADG,	    
    ISN_LOADAUTO,   
    ISN_LOADB,	    
    ISN_LOADW,	    
    ISN_LOADT,	    
    ISN_LOADGDICT,  
    ISN_LOADBDICT,  
    ISN_LOADWDICT,  
    ISN_LOADTDICT,  
    ISN_LOADS,	    
    ISN_LOADEXPORT, 
    ISN_LOADOUTER,  
    ISN_LOADSCRIPT, 
    ISN_LOADOPT,    
    ISN_LOADENV,    
    ISN_LOADREG,    

    ISN_STORE,	    
    ISN_STOREV,	    
    ISN_STOREG,	    
    ISN_STOREAUTO,  
    ISN_STOREB,	    
    ISN_STOREW,	    
    ISN_STORET,	    
    ISN_STORES,	    
    ISN_STOREEXPORT, 
    ISN_STOREOUTER,  
    ISN_STORESCRIPT, 
    ISN_STOREOPT,    
    ISN_STOREFUNCOPT, 
    ISN_STOREENV,    
    ISN_STOREREG,    
    

    ISN_STORENR,    
    ISN_STOREINDEX,	
			
    ISN_STORERANGE,	
			

    ISN_UNLET,		
    ISN_UNLETENV,	
    ISN_UNLETINDEX,	
    ISN_UNLETRANGE,	

    ISN_LOCKUNLOCK,	
    ISN_LOCKCONST,	

    
    ISN_PUSHNR,		
    ISN_PUSHBOOL,	
    ISN_PUSHSPEC,	
    ISN_PUSHF,		
    ISN_PUSHS,		
    ISN_PUSHBLOB,	
    ISN_PUSHFUNC,	
    ISN_PUSHCHANNEL,	
    ISN_PUSHJOB,	
    ISN_PUSHOBJ,	
    ISN_PUSHCLASS,	
    ISN_NEWLIST,	
			
    ISN_NEWDICT,	
			
    ISN_NEWPARTIAL,	

    ISN_AUTOLOAD,	

    
    ISN_BCALL,	    
    ISN_DCALL,	    
    ISN_METHODCALL, 
    ISN_UCALL,	    
    ISN_PCALL,	    
    ISN_PCALL_END,  
    ISN_RETURN,	    
    ISN_RETURN_VOID, 
    ISN_RETURN_OBJECT, 
    ISN_FUNCREF,    
    ISN_NEWFUNC,    
    ISN_DEF,	    
    ISN_DEFER,	    

    
    ISN_JUMP,	    
    ISN_JUMP_IF_ARG_SET, 
			 
    ISN_JUMP_IF_ARG_NOT_SET, 
			 

    
    ISN_FOR,	    
    ISN_WHILE,	    
		    
    ISN_ENDLOOP,    

    ISN_TRY,	    
    ISN_THROW,	    
    ISN_PUSHEXC,    
    ISN_CATCH,	    
    ISN_FINALLY,    
    ISN_ENDTRY,	    
    ISN_TRYCONT,    

    
    ISN_ADDLIST,    
    ISN_ADDBLOB,    

    
    ISN_OPNR,
    ISN_OPFLOAT,
    ISN_OPANY,

    
    ISN_COMPAREBOOL,
    ISN_COMPARESPECIAL,
    ISN_COMPARENULL,
    ISN_COMPARENR,
    ISN_COMPAREFLOAT,
    ISN_COMPARESTRING,
    ISN_COMPAREBLOB,
    ISN_COMPARELIST,
    ISN_COMPAREDICT,
    ISN_COMPAREFUNC,
    ISN_COMPAREANY,
    ISN_COMPAREOBJECT,

    
    ISN_CONCAT,     
    ISN_STRINDEX,   
    ISN_STRSLICE,   
    ISN_LISTAPPEND, 
    ISN_LISTINDEX,  
    ISN_LISTSLICE,  
    ISN_BLOBINDEX,  
    ISN_BLOBSLICE,  
    ISN_ANYINDEX,   
    ISN_ANYSLICE,   
    ISN_SLICE,	    
    ISN_BLOBAPPEND, 
    ISN_GETITEM,    
    ISN_MEMBER,	    
    ISN_STRINGMEMBER, 
    ISN_2BOOL,	    
    ISN_COND2BOOL,  
    ISN_2STRING,    
    ISN_2STRING_ANY, 
    ISN_NEGATENR,   

    ISN_CHECKTYPE,  
    ISN_CHECKLEN,   
    ISN_SETTYPE,    

    ISN_CLEARDICT,  
    ISN_USEDICT,    

    ISN_PUT,	    

    ISN_CMDMOD,	    
    ISN_CMDMOD_REV, 

    ISN_PROF_START, 
    ISN_PROF_END,   

    ISN_DEBUG,	    

    ISN_UNPACK,	    
    ISN_SHUFFLE,    
    ISN_DROP,	    

    ISN_REDIRSTART, 
    ISN_REDIREND,   

    ISN_CEXPR_AUCMD, 
    ISN_CEXPR_CORE,  

    ISN_FINISH	    
} isntype_T;



typedef struct {
    int	    cbf_idx;	    
    int	    cbf_argcount;   
} cbfunc_T;


typedef struct {
    int	    cdf_idx;	    
    int	    cdf_argcount;   
} cdfunc_T;


typedef struct {
    class_T *cmf_itf;	    
    int	    cmf_idx;	    
    int	    cmf_argcount;   
} cmfunc_T;


typedef struct {
    int	    cpf_top;	    
    int	    cpf_argcount;   
} cpfunc_T;


typedef struct {
    char_u  *cuf_name;
    int	    cuf_argcount;   
} cufunc_T;


typedef struct {
    varnumber_T	gi_index;
    int		gi_with_op;
} getitem_T;

typedef enum {
    JUMP_ALWAYS,
    JUMP_NEVER,
    JUMP_IF_FALSE,		
    JUMP_WHILE_FALSE,		
    JUMP_AND_KEEP_IF_TRUE,	
    JUMP_IF_COND_TRUE,		
    JUMP_IF_COND_FALSE,		
} jumpwhen_T;


typedef struct {
    jumpwhen_T	jump_when;
    int		jump_where;	
} jump_T;


typedef struct {
    int		jump_arg_off;	
    int		jump_where;	
} jumparg_T;


typedef struct {
    short	for_loop_idx;	
    int		for_end;	
} forloop_T;


typedef struct {
    short	while_funcref_idx;  
    int		while_end;	    
} whileloop_T;


typedef struct {
    short    end_funcref_idx;	
    short    end_depth;		
    short    end_var_idx;	
    short    end_var_count;	
} endloop_T;


typedef struct {
    int	    try_catch;	    
    int	    try_finally;    
    int	    try_endtry;	    
} tryref_T;


typedef struct {
    tryref_T *try_ref;
} try_T;


typedef struct {
    int	    tct_levels;	    
    int	    tct_where;	    
} trycont_T;


typedef struct {
    int	    echo_with_white;    
    int	    echo_count;		
} echo_T;


typedef struct {
    exprtype_T	op_type;
    int		op_ic;	    
} opexpr_T;


typedef struct {
    type_T	*ct_type;
    int8_T	ct_off;		
    int8_T	ct_arg_idx;	
    int8_T	ct_is_var;	
} checktype_T;


typedef struct {
    int		stnr_idx;
    varnumber_T	stnr_val;
} storenr_T;


typedef struct {
    char_u	*so_name;
    int		so_flags;
} storeopt_T;


typedef struct {
    char_u	*ls_name;	
    int		ls_sid;		
} loadstore_T;


typedef struct {
    int		sref_sid;	
    int		sref_idx;	
    int		sref_seq;	
    type_T	*sref_type;	
} scriptref_T;

typedef struct {
    scriptref_T	*scriptref;
} script_T;


typedef struct {
    char_u	*ul_name;	
    int		ul_forceit;	
} unlet_T;


typedef struct {
    char_u	  *fre_func_name;	
    loopvarinfo_T fre_loopvar_info;	
    class_T	  *fre_class;		
    int		  fre_object_method;	
    int		  fre_method_idx;	
} funcref_extra_T;


typedef struct {
    int		    fr_dfunc_idx;   
    funcref_extra_T *fr_extra;	    
} funcref_T;


typedef struct {
    char_u	  *nfa_lambda;	    
    char_u	  *nfa_global;	    
    loopvarinfo_T nfa_loopvar_info; 
} newfuncarg_T;

typedef struct {
    newfuncarg_T *nf_arg;
} newfunc_T;


typedef struct {
    int		cl_min_len;	
    int		cl_more_OK;	
} checklen_T;


typedef struct {
    int		shfl_item;	
    int		shfl_up;	
} shuffle_T;


typedef struct {
    int		put_regname;	
    linenr_T	put_lnum;	
} put_T;


typedef struct {
    cmdmod_T	*cf_cmdmod;	
} cmod_T;


typedef struct {
    int		unp_count;	
    int		unp_semicolon;	
} unpack_T;


typedef struct {
    int		outer_idx;	
    int		outer_depth;	
} isn_outer_T;

#define OUTER_LOOP_DEPTH -9	


typedef struct {
    char_u	*subs_cmd;	
    isn_T	*subs_instr;	
} subs_T;


typedef struct {
    int		cer_cmdidx;
    char_u	*cer_cmdline;
    int		cer_forceit;
} cexprref_T;


typedef struct {
    cexprref_T *cexpr_ref;
} cexpr_T;


typedef struct {
    int		offset;
    int		tolerant;
} tostring_T;


typedef struct {
    int		offset;
    int		invert;
} tobool_T;


typedef struct {
    varnumber_T	dbg_var_names_len;  
    int		dbg_break_lnum;	    
} debug_T;


typedef struct {
    int		defer_var_idx;	    
    int		defer_argcount;	    
} deferins_T;


typedef struct {
    int		ewin_count;	    
    long	ewin_time;	    
} echowin_T;


typedef struct {
    int		construct_size;	    
    class_T	*construct_class;   
} construct_T;


typedef struct {
    class_T	*cm_class;
    int		cm_idx;
} classmember_T;


typedef struct {
    vartype_T	si_vartype;
    class_T	*si_class;
} storeindex_T;


typedef struct {
    char_u	*lu_string;	
    class_T	*lu_cl_exec;	
    int		lu_is_arg;	
} lockunlock_T;


struct isn_S {
    isntype_T	isn_type;
    int		isn_lnum;
    union {
	char_u		    *string;
	varnumber_T	    number;
	blob_T		    *blob;
	vartype_T	    vartype;
	float_T		    fnumber;
	channel_T	    *channel;
	job_T		    *job;
	partial_T	    *partial;
	class_T		    *classarg;
	jump_T		    jump;
	jumparg_T	    jumparg;
	forloop_T	    forloop;
	whileloop_T	    whileloop;
	endloop_T	    endloop;
	try_T		    tryref;
	trycont_T	    trycont;
	cbfunc_T	    bfunc;
	cdfunc_T	    dfunc;
	cmfunc_T	    *mfunc;
	cpfunc_T	    pfunc;
	cufunc_T	    ufunc;
	echo_T		    echo;
	opexpr_T	    op;
	checktype_T	    type;
	storenr_T	    storenr;
	storeopt_T	    storeopt;
	loadstore_T	    loadstore;
	script_T	    script;
	unlet_T		    unlet;
	funcref_T	    funcref;
	newfunc_T	    newfunc;
	checklen_T	    checklen;
	shuffle_T	    shuffle;
	put_T		    put;
	cmod_T		    cmdmod;
	unpack_T	    unpack;
	isn_outer_T	    outer;
	subs_T		    subs;
	cexpr_T		    cexpr;
	isn_T		    *instr;
	tostring_T	    tostring;
	tobool_T	    tobool;
	getitem_T	    getitem;
	debug_T		    debug;
	deferins_T	    defer;
	echowin_T	    echowin;
	construct_T	    construct;
	classmember_T	    classmember;
	storeindex_T	    storeindex;
	lockunlock_T	    lockunlock;
    } isn_arg;
};


struct dfunc_S {
    ufunc_T	*df_ufunc;	    
    int		df_refcount;	    
    int		df_idx;		    
    char	df_deleted;	    
    char	df_delete_busy;	    
				    
    int		df_script_seq;	    
				    
    char_u	*df_name;	    

    garray_T	df_def_args_isn;    
    garray_T	df_var_names;	    

    
    isn_T	*df_instr;	    
    int		df_instr_count;	    
    int		df_instr_debug_count; 
    isn_T	*df_instr_debug;      
#ifdef FEAT_PROFILE
    isn_T	*df_instr_prof;	     
    int		df_instr_prof_count; 
#endif

    int		df_varcount;	    
    int		df_has_closure;	    
    int		df_defer_var_idx;   
				    
				    
};








#define STACK_FRAME_FUNC_OFF 0
#define STACK_FRAME_IIDX_OFF 1
#define STACK_FRAME_INSTR_OFF 2
#define STACK_FRAME_OUTER_OFF 3
#define STACK_FRAME_FUNCLOCAL_OFF 4
#define STACK_FRAME_IDX_OFF 5
#define STACK_FRAME_SIZE 6


extern garray_T def_functions;


#define LNUM_VARIABLE_RANGE (-999)


#define LNUM_VARIABLE_RANGE_ABOVE (-888)


#ifdef FEAT_PROFILE
# define INSTRUCTIONS(dfunc) \
	(debug_break_level > 0 || may_break_in_function(dfunc->df_ufunc) \
	    ? (dfunc)->df_instr_debug \
	    : ((do_profiling == PROF_YES && (dfunc->df_ufunc)->uf_profiling) \
		? (dfunc)->df_instr_prof \
		: (dfunc)->df_instr))
#else
# define INSTRUCTIONS(dfunc) \
	(debug_break_level > 0 || may_break_in_function((dfunc)->df_ufunc) \
		? (dfunc)->df_instr_debug \
		: (dfunc)->df_instr)
#endif







#define PPSIZE 50
typedef struct {
    typval_T	pp_tv[PPSIZE];	
    int		pp_used;	
    int		pp_is_const;	
				
} ppconst_T;


typedef enum {
    SKIP_NOT,		
    SKIP_YES,		
    SKIP_UNKNOWN	
} skip_T;


typedef struct endlabel_S endlabel_T;
struct endlabel_S {
    endlabel_T	*el_next;	    
    int		el_end_label;	    
};


typedef struct {
    int		is_seen_else;
    int		is_seen_skip_not;   
    int		is_had_return;	    
    int		is_if_label;	    
    endlabel_T	*is_end_label;	    
} ifscope_T;


typedef struct {
    int	    li_local_count;	    
    int	    li_closure_count;	    
    int	    li_funcref_idx;	    
    int	    li_depth;		    
} loop_info_T;


typedef struct {
    int		ws_top_label;	    
    endlabel_T	*ws_end_label;	    
    loop_info_T ws_loop_info;	    
} whilescope_T;


typedef struct {
    int		fs_top_label;	    
    endlabel_T	*fs_end_label;	    
    loop_info_T	fs_loop_info;	    
} forscope_T;


typedef struct {
    int		ts_try_label;	    
    endlabel_T	*ts_end_label;	    
    int		ts_catch_label;	    
    int		ts_caught_all;	    
    int		ts_has_finally;	    
    int		ts_no_return;	    
} tryscope_T;

typedef enum {
    NO_SCOPE,
    IF_SCOPE,
    WHILE_SCOPE,
    FOR_SCOPE,
    TRY_SCOPE,
    BLOCK_SCOPE
} scopetype_T;


typedef struct scope_S scope_T;
struct scope_S {
    scope_T	*se_outer;	    
    scopetype_T se_type;
    int		se_local_count;	    
    skip_T	se_skip_save;	    
    int		se_loop_depth;	    
    union {
	ifscope_T	se_if;
	whilescope_T	se_while;
	forscope_T	se_for;
	tryscope_T	se_try;
    } se_u;
};


typedef struct {
    char_u	*lv_name;
    type_T	*lv_type;
    int		lv_idx;		
    int		lv_loop_depth;	
    int		lv_loop_idx;	
    int		lv_from_outer;	
    int		lv_const;	
				
				
    int		lv_arg;		
} lvar_T;


typedef enum {
    dest_local,
    dest_option,
    dest_func_option,
    dest_env,
    dest_global,
    dest_buffer,
    dest_window,
    dest_tab,
    dest_vimvar,
    dest_class_member,
    dest_script,
    dest_reg,
    dest_expr,
} assign_dest_T;



typedef struct {
    assign_dest_T   lhs_dest;	    

    char_u	    *lhs_name;	    
				    
    size_t	    lhs_varlen;	    
				    
    char_u	    *lhs_whole;	    
				    
    size_t	    lhs_varlen_total; 
				      
    char_u	    *lhs_dest_end;  
				    
    char_u	    *lhs_end;	    

    int		    lhs_has_index;  

    int		    lhs_new_local;  
    int		    lhs_opt_flags;  
    int		    lhs_vimvaridx;  

    lvar_T	    lhs_local_lvar; 
    lvar_T	    lhs_arg_lvar;   
    lvar_T	    *lhs_lvar;	    

    class_T	    *lhs_class;		    
    int		    lhs_classmember_idx;    

    int		    lhs_scriptvar_sid;
    int		    lhs_scriptvar_idx;

    int		    lhs_has_type;   
    type_T	    *lhs_type;
    int		    lhs_member_idx;    
    type_T	    *lhs_member_type;  

    int		    lhs_append;	    
} lhs_T;


struct cctx_S {
    ufunc_T	*ctx_ufunc;	    
    int		ctx_lnum;	    
    char_u	*ctx_line_start;    
    garray_T	ctx_instr;	    

    int		ctx_prev_lnum;	    
				    

    compiletype_T ctx_compile_type;

    garray_T	ctx_locals;	    

    int		ctx_has_closure;    
				    
    int		ctx_closure_count;  
				    

    skip_T	ctx_skip;
    scope_T	*ctx_scope;	    
    int		ctx_had_return;	    
    int		ctx_had_throw;	    

    cctx_T	*ctx_outer;	    
				    
    int		ctx_outer_used;	    

    garray_T	ctx_type_stack;	    
    garray_T	*ctx_type_list;	    

    int		ctx_has_cmdmod;	    

    lhs_T	ctx_redir_lhs;	    
				    
};


typedef enum {
    CA_NOT_SPECIAL,
    CA_SEARCHPAIR,	    
    CA_SUBSTITUTE,	    
} ca_special_T;


#define TVTT_DO_MEMBER	    1
#define TVTT_MORE_SPECIFIC  2	


#define DEF_USE_PT_ARGV	    1	
