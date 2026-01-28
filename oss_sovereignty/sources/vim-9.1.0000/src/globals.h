




EXTERN long	Rows			
#ifdef DO_INIT
# if defined(MSWIN)
		    = 25L
# else
		    = 24L
# endif
#endif
		    ;
EXTERN long	Columns INIT(= 80);	


EXTERN schar_T	*ScreenLines INIT(= NULL);
EXTERN sattr_T	*ScreenAttrs INIT(= NULL);
EXTERN colnr_T  *ScreenCols INIT(= NULL);
EXTERN unsigned	*LineOffset INIT(= NULL);
EXTERN char_u	*LineWraps INIT(= NULL);	


EXTERN u8char_T	*ScreenLinesUC INIT(= NULL);	
EXTERN u8char_T	*ScreenLinesC[MAX_MCO];		
EXTERN int	Screen_mco INIT(= 0);		
						



EXTERN schar_T	*ScreenLines2 INIT(= NULL);


EXTERN schar_T	*current_ScreenLine INIT(= NULL);


EXTERN int	screen_cur_row INIT(= 0);
EXTERN int	screen_cur_col INIT(= 0);

#ifdef FEAT_SEARCH_EXTRA

EXTERN match_T	screen_search_hl;


EXTERN linenr_T search_hl_has_cursor_lnum INIT(= 0);


EXTERN int	no_hlsearch INIT(= FALSE);
#endif

#ifdef FEAT_FOLDING
EXTERN foldinfo_T win_foldinfo;	
#endif



EXTERN int redrawing_for_callback INIT(= 0);


EXTERN short	*TabPageIdxs INIT(= NULL);

#ifdef FEAT_PROP_POPUP

EXTERN short	*popup_mask INIT(= NULL);
EXTERN short	*popup_mask_next INIT(= NULL);

EXTERN char	*popup_transparent INIT(= NULL);


EXTERN int	popup_mask_refresh INIT(= TRUE);


EXTERN tabpage_T *popup_mask_tab INIT(= NULL);



EXTERN int	screen_zindex INIT(= 0);
#endif

EXTERN int	screen_Rows INIT(= 0);	    
EXTERN int	screen_Columns INIT(= 0);   


EXTERN int	mod_mask INIT(= 0);		



EXTERN int	vgetc_mod_mask INIT(= 0);
EXTERN int	vgetc_char INIT(= 0);


EXTERN int	cmdline_row;

EXTERN int	redraw_cmdline INIT(= FALSE);	
EXTERN int	redraw_mode INIT(= FALSE);	
EXTERN int	clear_cmdline INIT(= FALSE);	
EXTERN int	mode_displayed INIT(= FALSE);	
EXTERN int	no_win_do_lines_ins INIT(= FALSE); 
#if defined(FEAT_CRYPT) || defined(FEAT_EVAL)
EXTERN int	cmdline_star INIT(= FALSE);	
#endif

EXTERN int	exec_from_reg INIT(= FALSE);	

EXTERN int	screen_cleared INIT(= FALSE);	


EXTERN colnr_T	dollar_vcol INIT(= -1);



EXTERN char_u	*edit_submode INIT(= NULL); 
EXTERN char_u	*edit_submode_pre INIT(= NULL); 
EXTERN char_u	*edit_submode_extra INIT(= NULL);
EXTERN hlf_T	edit_submode_highl;	


#ifdef FEAT_RIGHTLEFT
EXTERN int	cmdmsg_rl INIT(= FALSE);    
#endif
EXTERN int	msg_col;
EXTERN int	msg_row;
EXTERN int	msg_scrolled;	
				
EXTERN int	msg_scrolled_ign INIT(= FALSE);
				
				
				

EXTERN char_u	*keep_msg INIT(= NULL);	    
EXTERN int	keep_msg_attr INIT(= 0);    
EXTERN int	keep_msg_more INIT(= FALSE); 
EXTERN int	need_fileinfo INIT(= FALSE);
EXTERN int	msg_scroll INIT(= FALSE);   
EXTERN int	msg_didout INIT(= FALSE);   
EXTERN int	msg_didany INIT(= FALSE);   
EXTERN int	msg_nowait INIT(= FALSE);   
EXTERN int	emsg_off INIT(= 0);	    
					    
EXTERN int	info_message INIT(= FALSE); 
EXTERN int      msg_hist_off INIT(= FALSE); 
#ifdef FEAT_EVAL
EXTERN int	need_clr_eos INIT(= FALSE); 
					    
EXTERN int	emsg_skip INIT(= 0);	    
					    
EXTERN int	emsg_severe INIT(= FALSE);  
					    

EXTERN char_u	*emsg_assert_fails_msg INIT(= NULL);
EXTERN long	emsg_assert_fails_lnum INIT(= 0);
EXTERN char_u	*emsg_assert_fails_context INIT(= NULL);

EXTERN int	did_endif INIT(= FALSE);    
#endif
EXTERN int	did_emsg;		    
					    
#ifdef FEAT_EVAL
EXTERN int	did_emsg_silent INIT(= 0);  
					    
					    
EXTERN int	did_emsg_def;		    
					    
EXTERN int	did_emsg_cumul;		    
					    
EXTERN int	called_vim_beep;	    
EXTERN int	uncaught_emsg;		    
					    
#endif
EXTERN int	did_emsg_syntax;	    
					    
EXTERN int	called_emsg;		    
EXTERN int	in_echowindow;		    
EXTERN int	ex_exitval INIT(= 0);	    
EXTERN int	emsg_on_display INIT(= FALSE);	
EXTERN int	rc_did_emsg INIT(= FALSE);  

EXTERN int	no_wait_return INIT(= 0);   
EXTERN int	need_wait_return INIT(= 0); 
EXTERN int	did_wait_return INIT(= FALSE);	
						
EXTERN int	need_maketitle INIT(= TRUE); 

EXTERN int	quit_more INIT(= FALSE);    
#if defined(UNIX) || defined(VMS) || defined(MACOS_X)
EXTERN int	newline_on_exit INIT(= FALSE);	
EXTERN int	intr_char INIT(= 0);	    
#endif
#if (defined(UNIX) || defined(VMS)) && defined(FEAT_X11)
EXTERN int	x_no_connect INIT(= FALSE); 
# if defined(FEAT_CLIENTSERVER)
EXTERN int	x_force_connect INIT(= FALSE);	
						
						
# endif
#endif
EXTERN int	ex_keep_indent INIT(= FALSE); 
EXTERN int	vgetc_busy INIT(= 0);	      

EXTERN int	didset_vim INIT(= FALSE);	    
EXTERN int	didset_vimruntime INIT(= FALSE);    


EXTERN int	lines_left INIT(= -1);	    
EXTERN int	msg_no_more INIT(= FALSE);  
					    


EXTERN garray_T	exestack INIT5(0, 0, sizeof(estack_T), 50, NULL);
#define HAVE_SOURCING_INFO  (exestack.ga_data != NULL && exestack.ga_len > 0)

#define SOURCING_NAME (((estack_T *)exestack.ga_data)[exestack.ga_len - 1].es_name)

#define SOURCING_LNUM (((estack_T *)exestack.ga_data)[exestack.ga_len - 1].es_lnum)


EXTERN sctx_T	current_sctx
#ifdef FEAT_EVAL
    INIT4(0, 0, 0, 0);
#else
    INIT(= {0});
#endif

#ifdef FEAT_EVAL

EXTERN int	estack_compiling INIT(= FALSE);

EXTERN int	ex_nesting_level INIT(= 0);	
EXTERN int	debug_break_level INIT(= -1);	
EXTERN int	debug_did_msg INIT(= FALSE);	
EXTERN int	debug_tick INIT(= 0);		
EXTERN int	debug_backtrace_level INIT(= 0); 
# ifdef FEAT_PROFILE
EXTERN int	do_profiling INIT(= PROF_NONE);	
# endif
EXTERN garray_T script_items INIT5(0, 0, sizeof(scriptitem_T *), 20, NULL);
# define SCRIPT_ITEM(id)    (((scriptitem_T **)script_items.ga_data)[(id) - 1])
# define SCRIPT_ID_VALID(id)    ((id) > 0 && (id) <= script_items.ga_len)
# define SCRIPT_SV(id)		(SCRIPT_ITEM(id)->sn_vars)
# define SCRIPT_VARS(id)	(SCRIPT_SV(id)->sv_dict.dv_hashtab)

# define FUNCLINE(fp, j)	((char_u **)(fp->uf_lines.ga_data))[j]


EXTERN except_T *current_exception;


EXTERN int did_throw INIT(= FALSE);


EXTERN int need_rethrow INIT(= FALSE);


EXTERN int check_cstack INIT(= FALSE);


EXTERN int trylevel INIT(= 0);


EXTERN int force_abort INIT(= FALSE);


EXTERN msglist_T **msg_list INIT(= NULL);


EXTERN int suppress_errthrow INIT(= FALSE);


EXTERN except_T *caught_stack INIT(= NULL);


EXTERN int	may_garbage_collect INIT(= FALSE);
EXTERN int	want_garbage_collect INIT(= FALSE);
EXTERN int	garbage_collect_at_exit INIT(= FALSE);









#define t_unknown		(static_types[0])
#define t_const_unknown		(static_types[1])


#define t_any			(static_types[2])
#define t_const_any		(static_types[3])


#define t_void			(static_types[4])
#define t_const_void		(static_types[5])

#define t_bool			(static_types[6])
#define t_const_bool		(static_types[7])

#define t_null			(static_types[8])
#define t_const_null		(static_types[9])

#define t_none			(static_types[10])
#define t_const_none		(static_types[11])

#define t_number		(static_types[12])
#define t_const_number		(static_types[13])


#define t_number_bool		(static_types[14])
#define t_const_number_bool	(static_types[15])


#define t_number_float		(static_types[16])
#define t_const_number_float	(static_types[17])

#define t_float			(static_types[18])
#define t_const_float		(static_types[19])

#define t_string		(static_types[20])
#define t_const_string		(static_types[21])

#define t_blob			(static_types[22])
#define t_const_blob		(static_types[23])

#define t_blob_null		(static_types[24])
#define t_const_blob_null	(static_types[25])

#define t_job			(static_types[26])
#define t_const_job		(static_types[27])

#define t_channel		(static_types[28])
#define t_const_channel		(static_types[29])


#define t_number_or_string	(static_types[30])
#define t_const_number_or_string (static_types[31])


#define t_func_unknown		(static_types[32])
#define t_const_func_unknown	(static_types[33])


#define t_func_void		(static_types[34])
#define t_const_func_void	(static_types[35])

#define t_func_any		(static_types[36])
#define t_const_func_any	(static_types[37])

#define t_func_number		(static_types[38])
#define t_const_func_number	(static_types[39])

#define t_func_string		(static_types[40])
#define t_const_func_string	(static_types[41])

#define t_func_bool		(static_types[42])
#define t_const_func_bool	(static_types[43])


#define t_func_0_void		(static_types[44])
#define t_const_func_0_void	(static_types[45])

#define t_func_0_any		(static_types[46])
#define t_const_func_0_any	(static_types[47])

#define t_func_0_number		(static_types[48])
#define t_const_func_0_number	(static_types[49])

#define t_func_0_string		(static_types[50])
#define t_const_func_0_string	(static_types[51])

#define t_list_any		(static_types[52])
#define t_const_list_any	(static_types[53])

#define t_dict_any		(static_types[54])
#define t_const_dict_any	(static_types[55])

#define t_list_empty		(static_types[56])
#define t_const_list_empty	(static_types[57])

#define t_dict_empty		(static_types[58])
#define t_const_dict_empty	(static_types[59])

#define t_list_bool		(static_types[60])
#define t_const_list_bool	(static_types[61])

#define t_list_number		(static_types[62])
#define t_const_list_number	(static_types[63])

#define t_list_string		(static_types[64])
#define t_const_list_string	(static_types[65])

#define t_list_job		(static_types[66])
#define t_const_list_job	(static_types[67])

#define t_list_dict_any		(static_types[68])
#define t_const_list_dict_any	(static_types[69])

#define t_list_list_any		(static_types[70])
#define t_const_list_list_any	(static_types[71])

#define t_list_list_string	(static_types[72])
#define t_const_list_list_string (static_types[73])

#define t_dict_bool		(static_types[74])
#define t_const_dict_bool	(static_types[75])

#define t_dict_number		(static_types[76])
#define t_const_dict_number	(static_types[77])

#define t_dict_string		(static_types[78])
#define t_const_dict_string	(static_types[79])

#define t_super			(static_types[80])
#define t_const_super		(static_types[81])

#define t_object		(static_types[82])
#define t_const_object		(static_types[83])

#define t_class			(static_types[84])
#define t_const_class		(static_types[85])

EXTERN type_T static_types[86]
#ifdef DO_INIT
= {
    
    {VAR_UNKNOWN, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_UNKNOWN, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_ANY, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_ANY, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_VOID, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_VOID, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_BOOL, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_BOOL, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_SPECIAL, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_SPECIAL, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_SPECIAL, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_SPECIAL, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_NUMBER, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_NUMBER, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_NUMBER, 0, 0, TTFLAG_STATIC|TTFLAG_BOOL_OK, NULL, NULL, NULL},
    {VAR_NUMBER, 0, 0, TTFLAG_STATIC|TTFLAG_BOOL_OK|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_NUMBER, 0, 0, TTFLAG_STATIC|TTFLAG_FLOAT_OK, NULL, NULL, NULL},
    {VAR_NUMBER, 0, 0, TTFLAG_STATIC|TTFLAG_FLOAT_OK|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_FLOAT, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_FLOAT, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_STRING, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_STRING, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_BLOB, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_BLOB, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_BLOB, 0, 0, TTFLAG_STATIC, &t_void, NULL, NULL},
    {VAR_BLOB, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_void, NULL, NULL},

    
    {VAR_JOB, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_JOB, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_CHANNEL, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_CHANNEL, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_STRING, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_STRING, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_FUNC, -1, -1, TTFLAG_STATIC, &t_unknown, NULL, NULL},
    {VAR_FUNC, -1, -1, TTFLAG_STATIC|TTFLAG_CONST, &t_unknown, NULL, NULL},

    
    {VAR_FUNC, -1, 0, TTFLAG_STATIC, &t_void, NULL, NULL},
    {VAR_FUNC, -1, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_void, NULL, NULL},

    
    {VAR_FUNC, -1, 0, TTFLAG_STATIC, &t_any, NULL, NULL},
    {VAR_FUNC, -1, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_any, NULL, NULL},

    
    {VAR_FUNC, -1, 0, TTFLAG_STATIC, &t_number, NULL, NULL},
    {VAR_FUNC, -1, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_number, NULL, NULL},

    
    {VAR_FUNC, -1, 0, TTFLAG_STATIC, &t_string, NULL, NULL},
    {VAR_FUNC, -1, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_string, NULL, NULL},

    
    {VAR_FUNC, -1, 0, TTFLAG_STATIC, &t_bool, NULL, NULL},
    {VAR_FUNC, -1, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_bool, NULL, NULL},

    
    {VAR_FUNC, 0, 0, TTFLAG_STATIC, &t_void, NULL, NULL},
    {VAR_FUNC, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_void, NULL, NULL},

    
    {VAR_FUNC, 0, 0, TTFLAG_STATIC, &t_any, NULL, NULL},
    {VAR_FUNC, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_any, NULL, NULL},

    
    {VAR_FUNC, 0, 0, TTFLAG_STATIC, &t_number, NULL, NULL},
    {VAR_FUNC, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_number, NULL, NULL},

    
    {VAR_FUNC, 0, 0, TTFLAG_STATIC, &t_string, NULL, NULL},
    {VAR_FUNC, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_string, NULL, NULL},

    
    {VAR_LIST, 0, 0, TTFLAG_STATIC, &t_any, NULL, NULL},
    {VAR_LIST, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_any, NULL, NULL},

    
    {VAR_DICT, 0, 0, TTFLAG_STATIC, &t_any, NULL, NULL},
    {VAR_DICT, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_any, NULL, NULL},

    
    {VAR_LIST, 0, 0, TTFLAG_STATIC, &t_unknown, NULL, NULL},
    {VAR_LIST, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_unknown, NULL, NULL},

    
    {VAR_DICT, 0, 0, TTFLAG_STATIC, &t_unknown, NULL, NULL},
    {VAR_DICT, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_unknown, NULL, NULL},

    
    {VAR_LIST, 0, 0, TTFLAG_STATIC, &t_bool, NULL, NULL},
    {VAR_LIST, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_bool, NULL, NULL},

    
    {VAR_LIST, 0, 0, TTFLAG_STATIC, &t_number, NULL, NULL},
    {VAR_LIST, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_number, NULL, NULL},

    
    {VAR_LIST, 0, 0, TTFLAG_STATIC, &t_string, NULL, NULL},
    {VAR_LIST, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_string, NULL, NULL},

    
    {VAR_LIST, 0, 0, TTFLAG_STATIC, &t_job, NULL, NULL},
    {VAR_LIST, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_job, NULL, NULL},

    
    {VAR_LIST, 0, 0, TTFLAG_STATIC, &t_dict_any, NULL, NULL},
    {VAR_LIST, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_dict_any, NULL, NULL},

    
    {VAR_LIST, 0, 0, TTFLAG_STATIC, &t_list_any, NULL, NULL},
    {VAR_LIST, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_list_any, NULL, NULL},

    
    {VAR_LIST, 0, 0, TTFLAG_STATIC, &t_list_string, NULL, NULL},
    {VAR_LIST, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_list_string, NULL, NULL},

    
    {VAR_DICT, 0, 0, TTFLAG_STATIC, &t_bool, NULL, NULL},
    {VAR_DICT, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_bool, NULL, NULL},

    
    {VAR_DICT, 0, 0, TTFLAG_STATIC, &t_number, NULL, NULL},
    {VAR_DICT, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_number, NULL, NULL},

    
    {VAR_DICT, 0, 0, TTFLAG_STATIC, &t_string, NULL, NULL},
    {VAR_DICT, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_string, NULL, NULL},

    
    {VAR_CLASS, 0, 0, TTFLAG_STATIC, &t_bool, NULL, NULL},
    {VAR_CLASS, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, &t_bool, NULL, NULL},

    
    {VAR_OBJECT, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_OBJECT, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},

    
    {VAR_CLASS, 0, 0, TTFLAG_STATIC, NULL, NULL, NULL},
    {VAR_CLASS, 0, 0, TTFLAG_STATIC|TTFLAG_CONST, NULL, NULL, NULL},
}
#endif
;

EXTERN int	did_source_packages INIT(= FALSE);
#endif 



EXTERN char_u	hash_removed;


EXTERN int	scroll_region INIT(= FALSE); 
EXTERN int	t_colors INIT(= 0);	    


EXTERN int include_none INIT(= 0);	
EXTERN int include_default INIT(= 0);	
EXTERN int include_link INIT(= 0);	


EXTERN int	highlight_match INIT(= FALSE);	
EXTERN linenr_T	search_match_lines;		
EXTERN colnr_T	search_match_endcol;		
#ifdef FEAT_SEARCH_EXTRA
EXTERN linenr_T	search_first_line INIT(= 0);	  
EXTERN linenr_T	search_last_line INIT(= MAXLNUM); 
#endif

EXTERN int	no_smartcase INIT(= FALSE);	

EXTERN int	need_check_timestamps INIT(= FALSE); 
						     
EXTERN int	did_check_timestamps INIT(= FALSE); 
						    
EXTERN int	no_check_timestamps INIT(= 0);	

EXTERN int	highlight_attr[HLF_COUNT];  
#ifdef FEAT_STL_OPT
# define USER_HIGHLIGHT
#endif
#ifdef USER_HIGHLIGHT
EXTERN int	highlight_user[9];		
# ifdef FEAT_STL_OPT
EXTERN int	highlight_stlnc[9];		
#  ifdef FEAT_TERMINAL
EXTERN int	highlight_stlterm[9];		
EXTERN int	highlight_stltermnc[9];		
#  endif
# endif
#endif
#ifdef FEAT_TERMINAL
		
		
EXTERN int	skip_term_loop INIT(= FALSE);
#endif
#ifdef FEAT_GUI
EXTERN char_u	*use_gvimrc INIT(= NULL);	
#endif
EXTERN int	cterm_normal_fg_color INIT(= 0);
EXTERN int	cterm_normal_fg_bold INIT(= 0);
EXTERN int	cterm_normal_bg_color INIT(= 0);
EXTERN int	cterm_normal_ul_color INIT(= 0);
#ifdef FEAT_TERMGUICOLORS
EXTERN guicolor_T cterm_normal_fg_gui_color INIT(= INVALCOLOR);
EXTERN guicolor_T cterm_normal_bg_gui_color INIT(= INVALCOLOR);
EXTERN guicolor_T cterm_normal_ul_gui_color INIT(= INVALCOLOR);
#endif
#ifdef FEAT_TERMRESPONSE
EXTERN int	is_mac_terminal INIT(= FALSE);  
#endif

EXTERN int	autocmd_busy INIT(= FALSE);	
EXTERN int	autocmd_no_enter INIT(= FALSE); 
EXTERN int	autocmd_no_leave INIT(= FALSE); 
EXTERN int	tabpage_move_disallowed INIT(= FALSE);  

EXTERN int	modified_was_set;		
EXTERN int	did_filetype INIT(= FALSE);	
EXTERN int	keep_filetype INIT(= FALSE);	
						
						







EXTERN int	au_did_filetype INIT(= FALSE);



EXTERN bufref_T	au_new_curbuf INIT3(NULL, 0, 0);





EXTERN buf_T	*au_pending_free_buf INIT(= NULL);
EXTERN win_T	*au_pending_free_win INIT(= NULL);


EXTERN int	mouse_row;
EXTERN int	mouse_col;
EXTERN int	mouse_past_bottom INIT(= FALSE);
EXTERN int	mouse_past_eol INIT(= FALSE);	
EXTERN int	mouse_dragging INIT(= 0);	
						
#if defined(FEAT_MOUSE_DEC)

EXTERN int	WantQueryMouse INIT(= FALSE);
#endif

#ifdef FEAT_GUI



EXTERN int	need_mouse_correct INIT(= FALSE);


EXTERN linenr_T gui_prev_topline INIT(= 0);
# ifdef FEAT_DIFF
EXTERN int	gui_prev_topfill INIT(= 0);
# endif
#endif

# ifdef FEAT_MOUSESHAPE
EXTERN int	drag_status_line INIT(= FALSE);	
EXTERN int	postponed_mouseshape INIT(= FALSE); 
						    
EXTERN int	drag_sep_line INIT(= FALSE);	
# endif


#ifdef FEAT_DIFF

EXTERN int	diff_context INIT(= 6);		
EXTERN int	diff_foldcolumn INIT(= 2);	
EXTERN int	diff_need_scrollbind INIT(= FALSE);
#endif



EXTERN int	updating_screen INIT(= FALSE);



EXTERN int	redraw_not_allowed INIT(= FALSE);

#ifdef MESSAGE_QUEUE


EXTERN int	dont_parse_messages INIT(= FALSE);
#endif

#ifdef FEAT_MENU

EXTERN vimmenu_T	*root_menu INIT(= NULL);

EXTERN int	sys_menu INIT(= FALSE);
#endif

#ifdef FEAT_GUI
# ifdef FEAT_MENU

EXTERN vimmenu_T	*current_menu;


EXTERN int force_menu_update INIT(= FALSE);
# endif
# ifdef FEAT_GUI_TABLINE

EXTERN int	    current_tab;


EXTERN int	    current_tabmenu;
#  define TABLINE_MENU_CLOSE	1
#  define TABLINE_MENU_NEW	2
#  define TABLINE_MENU_OPEN	3
# endif


EXTERN int	current_scrollbar;
EXTERN long_u	scrollbar_value;


EXTERN int	found_reverse_arg INIT(= FALSE);


EXTERN char	*font_argument INIT(= NULL);

# ifdef FEAT_GUI_GTK

EXTERN char	*background_argument INIT(= NULL);


EXTERN char	*foreground_argument INIT(= NULL);
# endif


EXTERN volatile sig_atomic_t hold_gui_events INIT(= 0);


EXTERN int	new_pixel_width INIT(= 0);
EXTERN int	new_pixel_height INIT(= 0);


EXTERN int	gui_win_x INIT(= -1);
EXTERN int	gui_win_y INIT(= -1);
#endif

#ifdef FEAT_CLIPBOARD
EXTERN Clipboard_T clip_star;	
# ifdef FEAT_X11
EXTERN Clipboard_T clip_plus;	
# else
#  define clip_plus clip_star	
#  define ONE_CLIPBOARD
# endif

# define CLIP_UNNAMED      1
# define CLIP_UNNAMED_PLUS 2
EXTERN int	clip_unnamed INIT(= 0); 

EXTERN int	clip_autoselect_star INIT(= FALSE);
EXTERN int	clip_autoselect_plus INIT(= FALSE);
EXTERN int	clip_autoselectml INIT(= FALSE);
EXTERN int	clip_html INIT(= FALSE);
EXTERN regprog_T *clip_exclude_prog INIT(= NULL);
EXTERN int	clip_unnamed_saved INIT(= 0);
#endif


EXTERN win_T	*firstwin;		
EXTERN win_T	*lastwin;		
EXTERN win_T	*prevwin INIT(= NULL);	
#define ONE_WINDOW (firstwin == lastwin)
#define W_NEXT(wp) ((wp)->w_next)

EXTERN win_T	*curwin;	





#define AUCMD_WIN_COUNT 5

typedef struct {
  win_T	*auc_win;	
			
  int	auc_win_used;	
} aucmdwin_T;

EXTERN aucmdwin_T aucmd_win[AUCMD_WIN_COUNT];

#ifdef FEAT_PROP_POPUP
EXTERN win_T    *first_popupwin;		
EXTERN win_T	*popup_dragwin INIT(= NULL);	


EXTERN int	popup_visible INIT(= FALSE);


EXTERN int	popup_uses_mouse_move INIT(= FALSE);

EXTERN int	text_prop_frozen INIT(= 0);


EXTERN int	ignore_text_props INIT(= FALSE);
#endif



EXTERN int	pum_will_redraw INIT(= FALSE);


EXTERN frame_T	*topframe;	


EXTERN tabpage_T    *first_tabpage;
EXTERN tabpage_T    *curtab;
EXTERN tabpage_T    *lastused_tabpage;
EXTERN int	    redraw_tabline INIT(= FALSE);  


EXTERN buf_T	*firstbuf INIT(= NULL);	
EXTERN buf_T	*lastbuf INIT(= NULL);	
EXTERN buf_T	*curbuf INIT(= NULL);	



EXTERN int	mf_dont_release INIT(= FALSE);	


EXTERN alist_T	global_alist;		    
EXTERN int	max_alist_id INIT(= 0);	    
EXTERN int	arg_had_last INIT(= FALSE); 
					    

EXTERN int	ru_col;		
#ifdef FEAT_STL_OPT
EXTERN int	ru_wid;		
#endif
EXTERN int	sc_col;		

#ifdef TEMPDIRNAMES
# if defined(UNIX) && defined(HAVE_FLOCK) \
	&& (defined(HAVE_DIRFD) || defined(__hpux))
EXTERN DIR	*vim_tempdir_dp INIT(= NULL); 
# endif
EXTERN char_u	*vim_tempdir INIT(= NULL); 
					   
#endif


EXTERN int	starting INIT(= NO_SCREEN);
				
				
EXTERN int	exiting INIT(= FALSE);
				
				
				
EXTERN int	really_exiting INIT(= FALSE);
				
				
EXTERN int	v_dying INIT(= 0); 
EXTERN int	stdout_isatty INIT(= TRUE);	

#if defined(FEAT_AUTOCHDIR)
EXTERN int	test_autochdir INIT(= FALSE);
#endif
EXTERN char	*last_chdir_reason INIT(= NULL);
#if defined(EXITFREE)
EXTERN int	entered_free_all_mem INIT(= FALSE);
				
#endif

EXTERN volatile sig_atomic_t full_screen INIT(= FALSE);
				
				

EXTERN int	restricted INIT(= FALSE);
				
EXTERN int	secure INIT(= FALSE);
				
				
				

EXTERN int	textlock INIT(= 0);
				
				
				

EXTERN int	curbuf_lock INIT(= 0);
				
				
EXTERN int	allbuf_lock INIT(= 0);
				
				
				
				
#ifdef HAVE_SANDBOX
EXTERN int	sandbox INIT(= 0);
				
				
				
#endif

EXTERN int	silent_mode INIT(= FALSE);
				
				

EXTERN pos_T	VIsual;		
EXTERN int	VIsual_active INIT(= FALSE);
				
EXTERN int	VIsual_select INIT(= FALSE);
				
EXTERN int	VIsual_select_reg INIT(= 0);
				
EXTERN int	restart_VIsual_select INIT(= 0);
				
EXTERN int	VIsual_reselect;
				
				

EXTERN int	VIsual_mode INIT(= 'v');
				

EXTERN int	redo_VIsual_busy INIT(= FALSE);
				


EXTERN int	resel_VIsual_mode INIT(= NUL);	
EXTERN linenr_T	resel_VIsual_line_count;	
EXTERN colnr_T	resel_VIsual_vcol;		


EXTERN pos_T	where_paste_started;


EXTERN int     did_ai INIT(= FALSE);


EXTERN colnr_T	ai_col INIT(= 0);


EXTERN int     end_comment_pending INIT(= NUL);


EXTERN int     did_syncbind INIT(= FALSE);


EXTERN int	did_si INIT(= FALSE);


EXTERN int	can_si INIT(= FALSE);


EXTERN int	can_si_back INIT(= FALSE);

EXTERN int	old_indent INIT(= 0);	

EXTERN pos_T	saved_cursor		
#ifdef DO_INIT
		    = {0, 0, 0}
#endif
		    ;


EXTERN pos_T	Insstart;		
					




EXTERN pos_T	Insstart_orig;


EXTERN int	orig_line_count INIT(= 0);  
EXTERN int	vr_lines_changed INIT(= 0); 

#if defined(FEAT_X11) && defined(FEAT_XCLIPBOARD)

EXTERN JMP_BUF x_jump_env;
#endif


#define DBCS_JPN	932	
#define DBCS_JPNU	9932	
#define DBCS_KOR	949	
#define DBCS_KORU	9949	
#define DBCS_CHS	936	
#define DBCS_CHSU	9936	
#define DBCS_CHT	950	
#define DBCS_CHTU	9950	
#define DBCS_2BYTE	1	
#define DBCS_DEBUG	(-1)

EXTERN int	enc_dbcs INIT(= 0);		
						
EXTERN int	enc_unicode INIT(= 0);	
EXTERN int	enc_utf8 INIT(= FALSE);		
EXTERN int	enc_latin1like INIT(= TRUE);	
#if defined(MSWIN) || defined(FEAT_CYGWIN_WIN32_CLIPBOARD)


EXTERN int	enc_codepage INIT(= -1);
EXTERN int	enc_latin9 INIT(= FALSE);	
#endif
EXTERN int	has_mbyte INIT(= 0);		


EXTERN char	mb_bytelen_tab[256];



EXTERN vimconv_T input_conv;			
EXTERN vimconv_T output_conv;			




EXTERN int (*mb_ptr2len)(char_u *p) INIT(= latin_ptr2len);


EXTERN int (*mb_ptr2len_len)(char_u *p, int size) INIT(= latin_ptr2len_len);


EXTERN int (*mb_char2len)(int c) INIT(= latin_char2len);



EXTERN int (*mb_char2bytes)(int c, char_u *buf) INIT(= latin_char2bytes);

EXTERN int (*mb_ptr2cells)(char_u *p) INIT(= latin_ptr2cells);
EXTERN int (*mb_ptr2cells_len)(char_u *p, int size) INIT(= latin_ptr2cells_len);
EXTERN int (*mb_char2cells)(int c) INIT(= latin_char2cells);
EXTERN int (*mb_off2cells)(unsigned off, unsigned max_off) INIT(= latin_off2cells);
EXTERN int (*mb_ptr2char)(char_u *p) INIT(= latin_ptr2char);




EXTERN int (*mb_head_off)(char_u *base, char_u *p) INIT(= latin_head_off);

# if defined(USE_ICONV) && defined(DYNAMIC_ICONV)

EXTERN size_t (*iconv) (iconv_t cd, const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft);
EXTERN iconv_t (*iconv_open) (const char *tocode, const char *fromcode);
EXTERN int (*iconv_close) (iconv_t cd);
EXTERN int (*iconvctl) (iconv_t cd, int request, void *argument);
EXTERN int* (*iconv_errno) (void);
# endif


#ifdef FEAT_XIM
# ifdef FEAT_GUI_GTK
EXTERN GtkIMContext	*xic INIT(= NULL);

EXTERN colnr_T		preedit_start_col INIT(= MAXCOL);
EXTERN colnr_T		preedit_end_col INIT(= MAXCOL);



EXTERN int		xim_changed_while_preediting INIT(= FALSE);
# else
EXTERN XIC		xic INIT(= NULL);
# endif
# ifdef FEAT_GUI
EXTERN guicolor_T	xim_fg_color INIT(= INVALCOLOR);
EXTERN guicolor_T	xim_bg_color INIT(= INVALCOLOR);
# endif
#endif


EXTERN int	State INIT(= MODE_NORMAL);

#ifdef FEAT_EVAL
EXTERN int	debug_mode INIT(= FALSE);
#endif

EXTERN int	finish_op INIT(= FALSE);
EXTERN long	opcount INIT(= 0);	
EXTERN int	motion_force INIT(= 0); 


EXTERN int exmode_active INIT(= 0);	


EXTERN int pending_exmode_active INIT(= FALSE);

EXTERN int ex_no_reprint INIT(= FALSE); 

EXTERN int reg_recording INIT(= 0);	
EXTERN int reg_executing INIT(= 0);	

EXTERN int pending_end_reg_executing INIT(= FALSE);



EXTERN int seenModifyOtherKeys INIT(= FALSE);


typedef enum {
    
    MOKS_INITIAL,
    
    MOKS_OFF,
    
    MOKS_ENABLED,
    
    
    
    MOKS_DISABLED,
    
    MOKS_AFTER_T_TE,
} mokstate_T;



EXTERN mokstate_T modify_otherkeys_state INIT(= MOKS_INITIAL);


typedef enum {
    
    KKPS_INITIAL,
    
    KKPS_OFF,
    
    KKPS_ENABLED,
    
    
    
    KKPS_DISABLED,
    
    KKPS_AFTER_T_TE,
} kkpstate_T;

EXTERN kkpstate_T kitty_protocol_state INIT(= KKPS_INITIAL);

EXTERN int no_mapping INIT(= FALSE);	
EXTERN int no_zero_mapping INIT(= 0);	
EXTERN int allow_keys INIT(= FALSE);	
					
EXTERN int no_reduce_keys INIT(= FALSE);  
					  
EXTERN int no_u_sync INIT(= 0);		
#ifdef FEAT_EVAL
EXTERN int u_sync_once INIT(= 0);	
					
#endif

EXTERN int restart_edit INIT(= 0);	
EXTERN int arrow_used;			
					
					
					
EXTERN int	ins_at_eol INIT(= FALSE); 
					  

EXTERN int	no_abbr INIT(= TRUE);	

#ifdef USE_EXE_NAME
EXTERN char_u	*exe_name;		
#endif

#ifdef USE_ON_FLY_SCROLL
EXTERN int	dont_scroll INIT(= FALSE);
#endif
EXTERN int	mapped_ctrl_c INIT(= FALSE); 
EXTERN int	ctrl_c_interrupts INIT(= TRUE);	

EXTERN cmdmod_T	cmdmod;			
EXTERN int	sticky_cmdmod_flags INIT(= 0); 

#ifdef FEAT_EVAL
EXTERN int	is_export INIT(= FALSE);    
#endif

EXTERN int	msg_silent INIT(= 0);	
EXTERN int	emsg_silent INIT(= 0);	
#ifdef FEAT_EVAL
EXTERN int	emsg_silent_def INIT(= 0);  
					    
#endif
EXTERN int	emsg_noredir INIT(= 0);	
EXTERN int	cmd_silent INIT(= FALSE); 

EXTERN int	in_assert_fails INIT(= FALSE);	

EXTERN int	swap_exists_action INIT(= SEA_NONE);
					
					
EXTERN int	swap_exists_did_quit INIT(= FALSE);
					

EXTERN char_u	*IObuff;		
					
EXTERN char_u	*NameBuff;		
					
EXTERN char	msg_buf[MSG_BUF_LEN];	


EXTERN int	RedrawingDisabled INIT(= 0);

EXTERN int	readonlymode INIT(= FALSE); 
EXTERN int	recoverymode INIT(= FALSE); 

EXTERN typebuf_T typebuf		
#ifdef DO_INIT
		    = {NULL, NULL, 0, 0, 0, 0, 0, 0, 0}
#endif
		    ;


EXTERN int	typebuf_was_empty INIT(= FALSE);

EXTERN int	ex_normal_busy INIT(= 0);   
#ifdef FEAT_EVAL
EXTERN int	in_feedkeys INIT(= 0);	    
#endif
EXTERN int	ex_normal_lock INIT(= 0);   

#ifdef FEAT_EVAL
EXTERN int	ignore_script INIT(= FALSE);  
#endif
EXTERN int	stop_insert_mode;	

EXTERN int	KeyTyped;		
EXTERN int	KeyStuffed;		
#ifdef HAVE_INPUT_METHOD
EXTERN int	vgetc_im_active;	
					
#endif
EXTERN int	maptick INIT(= 0);	

EXTERN int	must_redraw INIT(= 0);	    
EXTERN int	skip_redraw INIT(= FALSE);  
EXTERN int	do_redraw INIT(= FALSE);    
#ifdef FEAT_DIFF
EXTERN int	need_diff_redraw INIT(= 0); 
#endif
#ifdef FEAT_RELTIME

EXTERN int	redrawtime_limit_set INIT(= FALSE);
#endif

EXTERN int	need_highlight_changed INIT(= TRUE);

#define NSCRIPT 15
EXTERN FILE	*scriptin[NSCRIPT];	    
EXTERN int	curscript INIT(= 0);	    
EXTERN FILE	*scriptout  INIT(= NULL);   
EXTERN int	read_cmd_fd INIT(= 0);	    



EXTERN volatile sig_atomic_t got_int INIT(= FALSE);



EXTERN volatile sig_atomic_t got_sigusr1 INIT(= FALSE);

#ifdef USE_TERM_CONSOLE
EXTERN int	term_console INIT(= FALSE); 
#endif
EXTERN int	termcap_active INIT(= FALSE);	
EXTERN tmode_T	cur_tmode INIT(= TMODE_COOK);	
EXTERN int	bangredo INIT(= FALSE);	    
EXTERN int	searchcmdlen;		    
#ifdef FEAT_SYN_HL
EXTERN int	reg_do_extmatch INIT(= 0);  
					    
					    
EXTERN reg_extmatch_T *re_extmatch_in INIT(= NULL); 
					    
EXTERN reg_extmatch_T *re_extmatch_out INIT(= NULL); 
					    
#endif

EXTERN int	did_outofmem_msg INIT(= FALSE);
					    
EXTERN int	did_swapwrite_msg INIT(= FALSE);
					    
EXTERN int	undo_off INIT(= FALSE);	    
EXTERN int	global_busy INIT(= 0);	    
EXTERN int	listcmd_busy INIT(= FALSE); 
					    
EXTERN int	need_start_insertmode INIT(= FALSE);
					    
#if defined(FEAT_EVAL) || defined(PROTO)
EXTERN char_u	last_mode[MODE_MAX_LENGTH] INIT(= "n"); 
#endif
EXTERN char_u	*last_cmdline INIT(= NULL); 
EXTERN char_u	*repeat_cmdline INIT(= NULL); 
EXTERN char_u	*new_last_cmdline INIT(= NULL);	
						
EXTERN char_u	*autocmd_fname INIT(= NULL); 
EXTERN int	autocmd_fname_full;	     
EXTERN int	autocmd_bufnr INIT(= 0);     
EXTERN char_u	*autocmd_match INIT(= NULL); 
EXTERN int	aucmd_cmdline_changed_count INIT(= 0);

EXTERN int	did_cursorhold INIT(= FALSE); 
EXTERN pos_T	last_cursormoved	      
# ifdef DO_INIT
		    = {0, 0, 0}
# endif
		    ;

EXTERN int	postponed_split INIT(= 0);  
EXTERN int	postponed_split_flags INIT(= 0);  
EXTERN int	postponed_split_tab INIT(= 0);  
#ifdef FEAT_QUICKFIX
EXTERN int	g_do_tagpreview INIT(= 0);  
					    
#endif
EXTERN int	g_tag_at_cursor INIT(= FALSE); 
					    
					    

EXTERN int	replace_offset INIT(= 0);   

EXTERN char_u	*escape_chars INIT(= (char_u *)" \t\\\"|");
					    

EXTERN int	keep_help_flag INIT(= FALSE); 


EXTERN char_u	*empty_option INIT(= (char_u *)"");

EXTERN int  redir_off INIT(= FALSE);	
EXTERN FILE *redir_fd INIT(= NULL);	
#ifdef FEAT_EVAL
EXTERN int  redir_reg INIT(= 0);	
EXTERN int  redir_vname INIT(= 0);	
EXTERN int  redir_execute INIT(= 0);	
#endif

#ifdef FEAT_LANGMAP
EXTERN char_u	langmap_mapchar[256];	
#endif

EXTERN int  save_p_ls INIT(= -1);	
EXTERN int  save_p_wmh INIT(= -1);	
EXTERN int  wild_menu_showing INIT(= 0);
#define WM_SHOWN	1		
#define WM_SCROLLED	2		

#ifdef MSWIN
EXTERN char_u	toupper_tab[256];	
EXTERN char_u	tolower_tab[256];	
EXTERN int	found_register_arg INIT(= FALSE);
#endif

#ifdef FEAT_LINEBREAK
EXTERN char	breakat_flags[256];	
#endif


extern char *Version;
#if defined(HAVE_DATE_TIME) && defined(VMS) && defined(VAXC)
extern char longVersion[];
#else
extern char *longVersion;
#endif


#ifdef HAVE_PATHDEF
extern char_u *default_vim_dir;
extern char_u *default_vimruntime_dir;
extern char_u *all_cflags;
extern char_u *all_lflags;
# ifdef VMS
extern char_u *compiler_version;
extern char_u *compiled_arch;
# endif
extern char_u *compiled_user;
extern char_u *compiled_sys;
#endif

EXTERN char_u	*homedir INIT(= NULL);




EXTERN char_u	*globaldir INIT(= NULL);

#ifdef FEAT_FOLDING
EXTERN int	disable_fold_update INIT(= 0);
#endif


EXTERN int	km_stopsel INIT(= FALSE);
EXTERN int	km_startsel INIT(= FALSE);

EXTERN int	cmdwin_type INIT(= 0);	
EXTERN int	cmdwin_result INIT(= 0); 

EXTERN char_u no_lines_msg[]	INIT(= N_("--No lines in buffer--"));

EXTERN char typename_unknown[]	INIT(= N_("unknown"));
EXTERN char typename_int[]	INIT(= N_("int"));
EXTERN char typename_longint[]	INIT(= N_("long int"));
EXTERN char typename_longlongint[]	INIT(= N_("long long int"));
EXTERN char typename_unsignedint[]	INIT(= N_("unsigned int"));
EXTERN char typename_unsignedlongint[]	INIT(= N_("unsigned long int"));
EXTERN char typename_unsignedlonglongint[]	INIT(= N_("unsigned long long int"));
EXTERN char typename_pointer[]	INIT(= N_("pointer"));
EXTERN char typename_percent[]	INIT(= N_("percent"));
EXTERN char typename_char[] INIT(= N_("char"));
EXTERN char typename_string[]	INIT(= N_("string"));
EXTERN char typename_float[]	INIT(= N_("float"));


EXTERN long	sub_nsubs;	
EXTERN linenr_T	sub_nlines;	

#ifdef FEAT_EVAL

EXTERN struct subs_expr_S	*substitute_instr INIT(= NULL);
#endif


EXTERN char_u	wim_flags[4];

#if defined(FEAT_STL_OPT)

# define STL_IN_ICON	1
# define STL_IN_TITLE	2
EXTERN int      stl_syntax INIT(= 0);
#endif

#if defined(FEAT_BEVAL) && !defined(NO_X11_INCLUDES)
EXTERN BalloonEval	*balloonEval INIT(= NULL);
EXTERN int		balloonEvalForTerm INIT(= FALSE);
# if defined(FEAT_NETBEANS_INTG)
EXTERN int bevalServers INIT(= 0);
#  define BEVAL_NETBEANS		0x01
# endif
#endif

#ifdef CURSOR_SHAPE

extern cursorentry_T shape_table[SHAPE_IDX_COUNT];
#endif

#ifdef FEAT_PRINTER

# define OPT_PRINT_TOP		0
# define OPT_PRINT_BOT		1
# define OPT_PRINT_LEFT		2
# define OPT_PRINT_RIGHT	3
# define OPT_PRINT_HEADERHEIGHT	4
# define OPT_PRINT_SYNTAX	5
# define OPT_PRINT_NUMBER	6
# define OPT_PRINT_WRAP		7
# define OPT_PRINT_DUPLEX	8
# define OPT_PRINT_PORTRAIT	9
# define OPT_PRINT_PAPER	10
# define OPT_PRINT_COLLATE	11
# define OPT_PRINT_JOBSPLIT	12
# define OPT_PRINT_FORMFEED	13

# define OPT_PRINT_NUM_OPTIONS	14

EXTERN option_table_T printer_opts[OPT_PRINT_NUM_OPTIONS]
# ifdef DO_INIT
    = {
	{"top",	TRUE, 0, NULL, 0, FALSE},
	{"bottom",	TRUE, 0, NULL, 0, FALSE},
	{"left",	TRUE, 0, NULL, 0, FALSE},
	{"right",	TRUE, 0, NULL, 0, FALSE},
	{"header",	TRUE, 0, NULL, 0, FALSE},
	{"syntax",	FALSE, 0, NULL, 0, FALSE},
	{"number",	FALSE, 0, NULL, 0, FALSE},
	{"wrap",	FALSE, 0, NULL, 0, FALSE},
	{"duplex",	FALSE, 0, NULL, 0, FALSE},
	{"portrait", FALSE, 0, NULL, 0, FALSE},
	{"paper",	FALSE, 0, NULL, 0, FALSE},
	{"collate",	FALSE, 0, NULL, 0, FALSE},
	{"jobsplit", FALSE, 0, NULL, 0, FALSE},
	{"formfeed", FALSE, 0, NULL, 0, FALSE},
    }
# endif
    ;


# define PRT_UNIT_NONE	-1
# define PRT_UNIT_PERC	0
# define PRT_UNIT_INCH	1
# define PRT_UNIT_MM	2
# define PRT_UNIT_POINT	3
# define PRT_UNIT_NAMES {"pc", "in", "mm", "pt"}
#endif

#if (defined(FEAT_PRINTER) && defined(FEAT_STL_OPT)) \
	    || defined(FEAT_GUI_TABLINE)

EXTERN linenr_T printer_page_num;
#endif

#ifdef FEAT_XCLIPBOARD

EXTERN char	*xterm_display INIT(= NULL);


EXTERN int	xterm_display_allocated INIT(= FALSE);


EXTERN Display	*xterm_dpy INIT(= NULL);
#endif
#if defined(FEAT_XCLIPBOARD) || defined(FEAT_GUI_X11)
EXTERN XtAppContext app_context INIT(= (XtAppContext)NULL);
#endif

#ifdef FEAT_GUI_GTK
EXTERN guint32	gtk_socket_id INIT(= 0);
EXTERN int	echo_wid_arg INIT(= FALSE);	
#endif

#ifdef FEAT_GUI_MSWIN

EXTERN long_u	win_socket_id INIT(= 0);
#endif

#if defined(FEAT_CLIENTSERVER) || defined(FEAT_EVAL)
EXTERN int	typebuf_was_filled INIT(= FALSE); 
						  
#endif

#ifdef FEAT_CLIENTSERVER
EXTERN char_u	*serverName INIT(= NULL);	
# ifdef FEAT_X11
EXTERN Window	commWindow INIT(= None);
EXTERN Window	clientWindow INIT(= None);
EXTERN Atom	commProperty INIT(= None);
EXTERN char_u	*serverDelayedStartName INIT(= NULL);
# else
#  ifdef PROTO
typedef int HWND;
#  endif
EXTERN HWND	clientWindow INIT(= 0);
# endif
#endif

#if defined(UNIX) || defined(VMS)
EXTERN int	term_is_xterm INIT(= FALSE);	
#endif

#ifdef BACKSLASH_IN_FILENAME
EXTERN char	psepc INIT(= '\\');	
EXTERN char	psepcN INIT(= '/');	

EXTERN char	pseps[2] INIT2('\\', 0);
#endif



EXTERN int	virtual_op INIT(= MAYBE);

#ifdef FEAT_SYN_HL

EXTERN disptick_T	display_tick INIT(= 0);
#endif

#ifdef FEAT_SPELL


EXTERN linenr_T		spell_redraw_lnum INIT(= 0);
#endif

#ifdef FEAT_CONCEAL

EXTERN int		need_cursor_line_redraw INIT(= FALSE);
#endif

#ifdef USE_MCH_ERRMSG

EXTERN garray_T error_ga
# ifdef DO_INIT
		    = {0, 0, 0, 0, NULL}
# endif
		    ;
#endif

#ifdef FEAT_NETBEANS_INTG
EXTERN char *netbeansArg INIT(= NULL);	
EXTERN int netbeansFireChanges INIT(= 1); 
EXTERN int netbeansForcedQuit INIT(= 0);
EXTERN int netbeansReadFile INIT(= 1);	
EXTERN int netbeansSuppressNoLines INIT(= 0); 
#endif


EXTERN char top_bot_msg[]   INIT(= N_("search hit TOP, continuing at BOTTOM"));
EXTERN char bot_top_msg[]   INIT(= N_("search hit BOTTOM, continuing at TOP"));

EXTERN char line_msg[]	    INIT(= N_(" line "));

#ifdef FEAT_CRYPT
EXTERN char need_key_msg[]  INIT(= N_("Need encryption key for \"%s\""));
#endif


#ifdef USE_XSMP
EXTERN int xsmp_icefd INIT(= -1);   
#endif

#ifdef STARTUPTIME
EXTERN FILE *time_fd INIT(= NULL);  
#endif


EXTERN int vim_ignored;
EXTERN char *vim_ignoredp;

#ifdef FEAT_EVAL

EXTERN alloc_id_T  alloc_fail_id INIT(= aid_none);

EXTERN int  alloc_fail_countdown INIT(= -1);

EXTERN int  alloc_fail_repeat INIT(= 0);


EXTERN int  disable_char_avail_for_testing INIT(= FALSE);
EXTERN int  disable_redraw_for_testing INIT(= FALSE);
EXTERN int  ignore_redraw_flag_for_testing INIT(= FALSE);
EXTERN int  nfa_fail_for_testing INIT(= FALSE);
EXTERN int  no_query_mouse_for_testing INIT(= FALSE);
EXTERN int  ui_delay_for_testing INIT(= 0);
EXTERN int  reset_term_props_on_termresponse INIT(= FALSE);
EXTERN int  disable_vterm_title_for_testing INIT(= FALSE);
EXTERN long override_sysinfo_uptime INIT(= -1);
EXTERN int  override_autoload INIT(= FALSE);
EXTERN int  ml_get_alloc_lines INIT(= FALSE);
EXTERN int  ignore_unreachable_code_for_testing INIT(= FALSE);

EXTERN int  in_free_unref_items INIT(= FALSE);
#endif

#ifdef FEAT_TIMERS
EXTERN int  did_add_timer INIT(= FALSE);
EXTERN int  timer_busy INIT(= 0);   
#endif
#ifdef FEAT_EVAL
EXTERN int  input_busy INIT(= 0);   

EXTERN lval_root_T	*lval_root INIT(= NULL);
#endif

#ifdef FEAT_BEVAL_TERM
EXTERN int  bevalexpr_due_set INIT(= FALSE);
EXTERN proftime_T bevalexpr_due;
#endif

#ifdef FEAT_EVAL
EXTERN time_T time_for_testing INIT(= 0);

EXTERN int echo_attr INIT(= 0);   


EXTERN int  did_echo_string_emsg INIT(= FALSE);


EXTERN int *eval_lavars_used INIT(= NULL);


EXTERN char windowsVersion[20] INIT(= {0});


EXTERN listitem_T range_list_item;


EXTERN evalarg_T EVALARG_EVALUATE
# ifdef DO_INIT
	= {EVAL_EVALUATE, 0, NULL, NULL, NULL, NULL, GA_EMPTY, GA_EMPTY, NULL,
			 {0, 0, (int)sizeof(char_u *), 20, NULL}, 0, NULL}
# endif
	;
#endif

#ifdef MSWIN
# ifdef PROTO
typedef int HINSTANCE;
# endif
EXTERN int ctrl_break_was_pressed INIT(= FALSE);
EXTERN HINSTANCE g_hinst INIT(= NULL);
#endif


#if defined(FEAT_JOB_CHANNEL)
EXTERN char *ch_part_names[]
# ifdef DO_INIT
		= {"sock", "out", "err", "in"}
# endif
		;


EXTERN int channel_need_redraw INIT(= FALSE);
#endif

#ifdef FEAT_EVAL


EXTERN int ch_log_output INIT(= FALSE);

EXTERN int did_repeated_msg INIT(= 0);
# define REPEATED_MSG_LOOKING	    1
# define REPEATED_MSG_SAFESTATE	    2
#endif



EXTERN optmagic_T magic_overruled INIT(= OPTION_MAGIC_NOT_SET);


EXTERN int skip_win_fix_cursor INIT(= FALSE);

EXTERN int skip_win_fix_scroll INIT(= FALSE);

EXTERN int skip_update_topline INIT(= FALSE);


#define SHOWCMD_BUFLEN (SHOWCMD_COLS + 1 + 30)
EXTERN char_u showcmd_buf[SHOWCMD_BUFLEN];
