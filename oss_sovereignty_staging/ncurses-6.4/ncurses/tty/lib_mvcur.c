 

 

 

 

 
#define COMPUTE_OVERHEAD	1	 

 
#define LONG_DIST		(8 - COMPUTE_OVERHEAD)

 
#define NOT_LOCAL(sp, fy, fx, ty, tx)	((tx > LONG_DIST) \
		 && (tx < screen_columns(sp) - 1 - LONG_DIST) \
		 && (abs(ty-fy) + abs(tx-fx) > LONG_DIST))

 

 

#include <curses.priv.h>
#include <ctype.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_mvcur.c,v 1.157 2022/08/20 18:28:58 tom Exp $")

#define WANT_CHAR(sp, y, x) NewScreen(sp)->_line[y].text[x]	 

#if NCURSES_SP_FUNCS
#define BAUDRATE(sp)	sp->_term->_baudrate	 
#else
#define BAUDRATE(sp)	cur_term->_baudrate	 
#endif

#if defined(MAIN) || defined(NCURSES_TEST)
#include <sys/time.h>

static bool profiling = FALSE;
static float diff;
#endif  

#undef NCURSES_OUTC_FUNC
#define NCURSES_OUTC_FUNC myOutCh

#define OPT_SIZE 512

static int normalized_cost(NCURSES_SP_DCLx const char *const cap, int affcnt);

 

#ifdef TRACE
static int
trace_cost_of(NCURSES_SP_DCLx const char *capname, const char *cap, int affcnt)
{
    int result = NCURSES_SP_NAME(_nc_msec_cost) (NCURSES_SP_ARGx cap, affcnt);
    TR(TRACE_CHARPUT | TRACE_MOVE,
       ("CostOf %s %d %s", capname, result, _nc_visbuf(cap)));
    return result;
}
#define CostOf(cap,affcnt) trace_cost_of(NCURSES_SP_ARGx #cap, cap, affcnt)

static int
trace_normalized_cost(NCURSES_SP_DCLx const char *capname, const char *cap, int affcnt)
{
    int result = normalized_cost(NCURSES_SP_ARGx cap, affcnt);
    TR(TRACE_CHARPUT | TRACE_MOVE,
       ("NormalizedCost %s %d %s", capname, result, _nc_visbuf(cap)));
    return result;
}
#define NormalizedCost(cap,affcnt) trace_normalized_cost(NCURSES_SP_ARGx #cap, cap, affcnt)

#else

#define CostOf(cap,affcnt) NCURSES_SP_NAME(_nc_msec_cost)(NCURSES_SP_ARGx cap, affcnt)
#define NormalizedCost(cap,affcnt) normalized_cost(NCURSES_SP_ARGx cap, affcnt)

#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(_nc_msec_cost) (NCURSES_SP_DCLx const char *const cap, int affcnt)
 
{
    if (cap == 0)
	return (INFINITY);
    else {
	const char *cp;
	float cum_cost = 0.0;

	for (cp = cap; *cp; cp++) {
	     
	    if (cp[0] == '$' && cp[1] == '<' && strchr(cp, '>')) {
		float number = 0.0;

		for (cp += 2; *cp != '>'; cp++) {
		    if (isdigit(UChar(*cp)))
			number = number * 10 + (float) (*cp - '0');
		    else if (*cp == '*')
			number *= (float) affcnt;
		    else if (*cp == '.' && (*++cp != '>') && isdigit(UChar(*cp)))
			number += (float) ((*cp - '0') / 10.0);
		}

#if NCURSES_NO_PADDING
		if (!GetNoPadding(SP_PARM))
#endif
		    cum_cost += number * 10;
	    } else if (SP_PARM) {
		cum_cost += (float) SP_PARM->_char_padding;
	    }
	}

	return ((int) cum_cost);
    }
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
_nc_msec_cost(const char *const cap, int affcnt)
{
    return NCURSES_SP_NAME(_nc_msec_cost) (CURRENT_SCREEN, cap, affcnt);
}
#endif

static int
normalized_cost(NCURSES_SP_DCLx const char *const cap, int affcnt)
 
{
    int cost = NCURSES_SP_NAME(_nc_msec_cost) (NCURSES_SP_ARGx cap, affcnt);
    if (cost != INFINITY)
	cost = (cost + SP_PARM->_char_padding - 1) / SP_PARM->_char_padding;
    return cost;
}

static void
reset_scroll_region(NCURSES_SP_DCL0)
 
{
    if (change_scroll_region) {
	NCURSES_PUTP2("change_scroll_region",
		      TIPARM_2(change_scroll_region,
			       0, screen_lines(SP_PARM) - 1));
    }
}

NCURSES_EXPORT(void)
NCURSES_SP_NAME(_nc_mvcur_resume) (NCURSES_SP_DCL0)
 
{
    if (!SP_PARM || !IsTermInfo(SP_PARM))
	return;

     
    if (enter_ca_mode) {
	NCURSES_PUTP2("enter_ca_mode", enter_ca_mode);
    }

     
    reset_scroll_region(NCURSES_SP_ARG);
    SP_PARM->_cursrow = SP_PARM->_curscol = -1;

     
    if (SP_PARM->_cursor != -1) {
	int cursor = SP_PARM->_cursor;
	SP_PARM->_cursor = -1;
	NCURSES_SP_NAME(curs_set) (NCURSES_SP_ARGx cursor);
    }
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
_nc_mvcur_resume(void)
{
    NCURSES_SP_NAME(_nc_mvcur_resume) (CURRENT_SCREEN);
}
#endif

NCURSES_EXPORT(void)
NCURSES_SP_NAME(_nc_mvcur_init) (NCURSES_SP_DCL0)
 
{
    if (SP_PARM->_ofp && NC_ISATTY(fileno(SP_PARM->_ofp))) {
	SP_PARM->_char_padding = ((BAUDBYTE * 1000 * 10)
				  / (BAUDRATE(SP_PARM) > 0
				     ? BAUDRATE(SP_PARM)
				     : 9600));
    } else {
	SP_PARM->_char_padding = 1;	 
    }
    if (SP_PARM->_char_padding <= 0)
	SP_PARM->_char_padding = 1;	 
    TR(TRACE_CHARPUT | TRACE_MOVE, ("char_padding %d msecs", SP_PARM->_char_padding));

     
    SP_PARM->_cr_cost = CostOf(carriage_return, 0);
    SP_PARM->_home_cost = CostOf(cursor_home, 0);
    SP_PARM->_ll_cost = CostOf(cursor_to_ll, 0);
#if USE_HARD_TABS
    if (getenv("NCURSES_NO_HARD_TABS") == 0
	&& dest_tabs_magic_smso == 0
	&& HasHardTabs()) {
	SP_PARM->_ht_cost = CostOf(tab, 0);
	SP_PARM->_cbt_cost = CostOf(back_tab, 0);
    } else {
	SP_PARM->_ht_cost = INFINITY;
	SP_PARM->_cbt_cost = INFINITY;
    }
#endif  
    SP_PARM->_cub1_cost = CostOf(cursor_left, 0);
    SP_PARM->_cuf1_cost = CostOf(cursor_right, 0);
    SP_PARM->_cud1_cost = CostOf(cursor_down, 0);
    SP_PARM->_cuu1_cost = CostOf(cursor_up, 0);

    SP_PARM->_smir_cost = CostOf(enter_insert_mode, 0);
    SP_PARM->_rmir_cost = CostOf(exit_insert_mode, 0);
    SP_PARM->_ip_cost = 0;
    if (insert_padding) {
	SP_PARM->_ip_cost = CostOf(insert_padding, 0);
    }

     
    SP_PARM->_address_cursor = cursor_address ? cursor_address : cursor_mem_address;

     
    SP_PARM->_cup_cost = CostOf(TIPARM_2(SP_PARM->_address_cursor, 23, 23), 1);
    SP_PARM->_cub_cost = CostOf(TIPARM_1(parm_left_cursor, 23), 1);
    SP_PARM->_cuf_cost = CostOf(TIPARM_1(parm_right_cursor, 23), 1);
    SP_PARM->_cud_cost = CostOf(TIPARM_1(parm_down_cursor, 23), 1);
    SP_PARM->_cuu_cost = CostOf(TIPARM_1(parm_up_cursor, 23), 1);
    SP_PARM->_hpa_cost = CostOf(TIPARM_1(column_address, 23), 1);
    SP_PARM->_vpa_cost = CostOf(TIPARM_1(row_address, 23), 1);

     
    SP_PARM->_ed_cost = NormalizedCost(clr_eos, 1);
    SP_PARM->_el_cost = NormalizedCost(clr_eol, 1);
    SP_PARM->_el1_cost = NormalizedCost(clr_bol, 1);
    SP_PARM->_dch1_cost = NormalizedCost(delete_character, 1);
    SP_PARM->_ich1_cost = NormalizedCost(insert_character, 1);

     
    if (back_color_erase)
	SP_PARM->_el_cost = 0;

     
    SP_PARM->_dch_cost = NormalizedCost(TIPARM_1(parm_dch, 23), 1);
    SP_PARM->_ich_cost = NormalizedCost(TIPARM_1(parm_ich, 23), 1);
    SP_PARM->_ech_cost = NormalizedCost(TIPARM_1(erase_chars, 23), 1);
    SP_PARM->_rep_cost = NormalizedCost(TIPARM_2(repeat_char, ' ', 23), 1);

    SP_PARM->_cup_ch_cost = NormalizedCost(TIPARM_2(SP_PARM->_address_cursor,
						    23, 23),
					   1);
    SP_PARM->_hpa_ch_cost = NormalizedCost(TIPARM_1(column_address, 23), 1);
    SP_PARM->_cuf_ch_cost = NormalizedCost(TIPARM_1(parm_right_cursor, 23), 1);
    SP_PARM->_inline_cost = min(SP_PARM->_cup_ch_cost,
				min(SP_PARM->_hpa_ch_cost,
				    SP_PARM->_cuf_ch_cost));

     
    if (save_cursor != 0
	&& enter_ca_mode != 0
	&& strstr(enter_ca_mode, save_cursor) != 0) {
	T(("...suppressed sc/rc capability due to conflict with smcup/rmcup"));
	save_cursor = 0;
	restore_cursor = 0;
    }

     
    NCURSES_SP_NAME(_nc_mvcur_resume) (NCURSES_SP_ARG);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
_nc_mvcur_init(void)
{
    NCURSES_SP_NAME(_nc_mvcur_init) (CURRENT_SCREEN);
}
#endif

NCURSES_EXPORT(void)
NCURSES_SP_NAME(_nc_mvcur_wrap) (NCURSES_SP_DCL0)
 
{
    if (!SP_PARM || !IsTermInfo(SP_PARM))
	return;

     
    TINFO_MVCUR(NCURSES_SP_ARGx -1, -1, screen_lines(SP_PARM) - 1, 0);

     
    if (SP_PARM->_cursor != -1) {
	int cursor = SP_PARM->_cursor;
	NCURSES_SP_NAME(curs_set) (NCURSES_SP_ARGx 1);
	SP_PARM->_cursor = cursor;
    }

    if (exit_ca_mode) {
	NCURSES_PUTP2("exit_ca_mode", exit_ca_mode);
    }
     
    NCURSES_SP_NAME(_nc_outch) (NCURSES_SP_ARGx '\r');
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
_nc_mvcur_wrap(void)
{
    NCURSES_SP_NAME(_nc_mvcur_wrap) (CURRENT_SCREEN);
}
#endif

 

 
static NCURSES_INLINE int
repeated_append(string_desc * target, int total, int num, int repeat, const char *src)
{
    size_t need = (size_t) repeat * strlen(src);

    if (need < target->s_size) {
	while (repeat-- > 0) {
	    if (_nc_safe_strcat(target, src)) {
		total += num;
	    } else {
		total = INFINITY;
		break;
	    }
	}
    } else {
	total = INFINITY;
    }
    return total;
}

#ifndef NO_OPTIMIZE
#define NEXTTAB(fr)	(fr + init_tabs - (fr % init_tabs))

 
#define LASTTAB(fr)	((fr > 0) ? ((fr - 1) / init_tabs) * init_tabs : -1)

static int
relative_move(NCURSES_SP_DCLx
	      string_desc * target,
	      int from_y,
	      int from_x,
	      int to_y,
	      int to_x,
	      int ovw)
 
{
    string_desc save;
    int n, vcost = 0, hcost = 0;

    (void) _nc_str_copy(&save, target);

    if (to_y != from_y) {
	vcost = INFINITY;

	if (row_address != 0
	    && _nc_safe_strcat(target, TIPARM_1(row_address, to_y))) {
	    vcost = SP_PARM->_vpa_cost;
	}

	if (to_y > from_y) {
	    n = (to_y - from_y);

	    if (parm_down_cursor
		&& SP_PARM->_cud_cost < vcost
		&& _nc_safe_strcat(_nc_str_copy(target, &save),
				   TIPARM_1(parm_down_cursor, n))) {
		vcost = SP_PARM->_cud_cost;
	    }

	    if (cursor_down
		&& (*cursor_down != '\n')
		&& (n * SP_PARM->_cud1_cost < vcost)) {
		vcost = repeated_append(_nc_str_copy(target, &save), 0,
					SP_PARM->_cud1_cost, n, cursor_down);
	    }
	} else {		 
	    n = (from_y - to_y);

	    if (parm_up_cursor
		&& SP_PARM->_cuu_cost < vcost
		&& _nc_safe_strcat(_nc_str_copy(target, &save),
				   TIPARM_1(parm_up_cursor, n))) {
		vcost = SP_PARM->_cuu_cost;
	    }

	    if (cursor_up && (n * SP_PARM->_cuu1_cost < vcost)) {
		vcost = repeated_append(_nc_str_copy(target, &save), 0,
					SP_PARM->_cuu1_cost, n, cursor_up);
	    }
	}

	if (vcost == INFINITY)
	    return (INFINITY);
    }

    save = *target;

    if (to_x != from_x) {
	char str[OPT_SIZE];
	string_desc check;

	hcost = INFINITY;

	if (column_address
	    && _nc_safe_strcat(_nc_str_copy(target, &save),
			       TIPARM_1(column_address, to_x))) {
	    hcost = SP_PARM->_hpa_cost;
	}

	if (to_x > from_x) {
	    n = to_x - from_x;

	    if (parm_right_cursor
		&& SP_PARM->_cuf_cost < hcost
		&& _nc_safe_strcat(_nc_str_copy(target, &save),
				   TIPARM_1(parm_right_cursor, n))) {
		hcost = SP_PARM->_cuf_cost;
	    }

	    if (cursor_right) {
		int lhcost = 0;

		(void) _nc_str_init(&check, str, sizeof(str));

#if USE_HARD_TABS
		 
		if (init_tabs > 0 && tab) {
		    int nxt, fr;

		    for (fr = from_x; (nxt = NEXTTAB(fr)) <= to_x; fr = nxt) {
			lhcost = repeated_append(&check, lhcost,
						 SP_PARM->_ht_cost, 1, tab);
			if (lhcost == INFINITY)
			    break;
		    }

		    n = to_x - fr;
		    from_x = fr;
		}
#endif  

		if (n <= 0 || n >= (int) check.s_size)
		    ovw = FALSE;
#if BSD_TPUTS
		 
		if (ovw
		    && n > 0
		    && n < (int) check.s_size
		    && vcost == 0
		    && str[0] == '\0') {
		    int wanted = CharOf(WANT_CHAR(SP_PARM, to_y, from_x));
		    if (is8bits(wanted) && isdigit(wanted))
			ovw = FALSE;
		}
#endif
		 
		if (ovw) {
		    int i;

		    for (i = 0; i < n; i++) {
			NCURSES_CH_T ch = WANT_CHAR(SP_PARM, to_y, from_x + i);
			if (!SameAttrOf(ch, SCREEN_ATTRS(SP_PARM))
#if USE_WIDEC_SUPPORT
			    || !Charable(ch)
#endif
			    ) {
			    ovw = FALSE;
			    break;
			}
		    }
		}
		if (ovw) {
		    int i;

		    for (i = 0; i < n; i++)
			*check.s_tail++ = (char) CharOf(WANT_CHAR(SP_PARM, to_y,
								  from_x + i));
		    *check.s_tail = '\0';
		    check.s_size -= (size_t) n;
		    lhcost += n * SP_PARM->_char_padding;
		} else {
		    lhcost = repeated_append(&check, lhcost, SP_PARM->_cuf1_cost,
					     n, cursor_right);
		}

		if (lhcost < hcost
		    && _nc_safe_strcat(_nc_str_copy(target, &save), str)) {
		    hcost = lhcost;
		}
	    }
	} else {		 
	    n = from_x - to_x;

	    if (parm_left_cursor
		&& SP_PARM->_cub_cost < hcost
		&& _nc_safe_strcat(_nc_str_copy(target, &save),
				   TIPARM_1(parm_left_cursor, n))) {
		hcost = SP_PARM->_cub_cost;
	    }

	    if (cursor_left) {
		int lhcost = 0;

		(void) _nc_str_init(&check, str, sizeof(str));

#if USE_HARD_TABS
		if (init_tabs > 0 && back_tab) {
		    int nxt, fr;

		    for (fr = from_x; (nxt = LASTTAB(fr)) >= to_x; fr = nxt) {
			lhcost = repeated_append(&check, lhcost,
						 SP_PARM->_cbt_cost,
						 1, back_tab);
			if (lhcost == INFINITY)
			    break;
		    }

		    n = fr - to_x;
		}
#endif  

		lhcost = repeated_append(&check, lhcost,
					 SP_PARM->_cub1_cost,
					 n, cursor_left);

		if (lhcost < hcost
		    && _nc_safe_strcat(_nc_str_copy(target, &save), str)) {
		    hcost = lhcost;
		}
	    }
	}

	if (hcost == INFINITY)
	    return (INFINITY);
    }

    return (vcost + hcost);
}
#endif  

 

static NCURSES_INLINE int
onscreen_mvcur(NCURSES_SP_DCLx
	       int yold, int xold,
	       int ynew, int xnew, int ovw,
	       NCURSES_SP_OUTC myOutCh)
 
{
    string_desc result;
    char buffer[OPT_SIZE];
    int tactic = 0, newcost, usecost = INFINITY;
    int t5_cr_cost;

#if defined(MAIN) || defined(NCURSES_TEST)
    struct timeval before, after;

    gettimeofday(&before, NULL);
#endif  

#define NullResult _nc_str_null(&result, sizeof(buffer))
#define InitResult _nc_str_init(&result, buffer, sizeof(buffer))

     
    if (_nc_safe_strcpy(InitResult, TIPARM_2(SP_PARM->_address_cursor,
					     ynew, xnew))) {
	tactic = 0;
	usecost = SP_PARM->_cup_cost;

#if defined(TRACE) || defined(NCURSES_TEST)
	if (!(_nc_optimize_enable & OPTIMIZE_MVCUR))
	    goto nonlocal;
#endif  

	 
	if (yold == -1 || xold == -1 || NOT_LOCAL(SP_PARM, yold, xold, ynew, xnew)) {
#if defined(MAIN) || defined(NCURSES_TEST)
	    if (!profiling) {
		(void) fputs("nonlocal\n", stderr);
		goto nonlocal;	 
	    }
#else
	    goto nonlocal;
#endif  
	}
    }
#ifndef NO_OPTIMIZE
     
    if (yold != -1 && xold != -1
	&& ((newcost = relative_move(NCURSES_SP_ARGx
				     NullResult,
				     yold, xold,
				     ynew, xnew, ovw)) != INFINITY)
	&& newcost < usecost) {
	tactic = 1;
	usecost = newcost;
    }

     
    if (yold != -1 && carriage_return
	&& ((newcost = relative_move(NCURSES_SP_ARGx
				     NullResult,
				     yold, 0,
				     ynew, xnew, ovw)) != INFINITY)
	&& SP_PARM->_cr_cost + newcost < usecost) {
	tactic = 2;
	usecost = SP_PARM->_cr_cost + newcost;
    }

     
    if (cursor_home
	&& ((newcost = relative_move(NCURSES_SP_ARGx
				     NullResult,
				     0, 0,
				     ynew, xnew, ovw)) != INFINITY)
	&& SP_PARM->_home_cost + newcost < usecost) {
	tactic = 3;
	usecost = SP_PARM->_home_cost + newcost;
    }

     
    if (cursor_to_ll
	&& ((newcost = relative_move(NCURSES_SP_ARGx
				     NullResult,
				     screen_lines(SP_PARM) - 1, 0,
				     ynew, xnew, ovw)) != INFINITY)
	&& SP_PARM->_ll_cost + newcost < usecost) {
	tactic = 4;
	usecost = SP_PARM->_ll_cost + newcost;
    }

     
    t5_cr_cost = (xold > 0 ? SP_PARM->_cr_cost : 0);
    if (auto_left_margin && !eat_newline_glitch
	&& yold > 0 && cursor_left
	&& ((newcost = relative_move(NCURSES_SP_ARGx
				     NullResult,
				     yold - 1, screen_columns(SP_PARM) - 1,
				     ynew, xnew, ovw)) != INFINITY)
	&& t5_cr_cost + SP_PARM->_cub1_cost + newcost < usecost) {
	tactic = 5;
	usecost = t5_cr_cost + SP_PARM->_cub1_cost + newcost;
    }

     
    if (tactic)
	InitResult;
    switch (tactic) {
    case 1:
	(void) relative_move(NCURSES_SP_ARGx
			     &result,
			     yold, xold,
			     ynew, xnew, ovw);
	break;
    case 2:
	(void) _nc_safe_strcpy(&result, carriage_return);
	(void) relative_move(NCURSES_SP_ARGx
			     &result,
			     yold, 0,
			     ynew, xnew, ovw);
	break;
    case 3:
	(void) _nc_safe_strcpy(&result, cursor_home);
	(void) relative_move(NCURSES_SP_ARGx
			     &result, 0, 0,
			     ynew, xnew, ovw);
	break;
    case 4:
	(void) _nc_safe_strcpy(&result, cursor_to_ll);
	(void) relative_move(NCURSES_SP_ARGx
			     &result,
			     screen_lines(SP_PARM) - 1, 0,
			     ynew, xnew, ovw);
	break;
    case 5:
	if (xold > 0)
	    (void) _nc_safe_strcat(&result, carriage_return);
	(void) _nc_safe_strcat(&result, cursor_left);
	(void) relative_move(NCURSES_SP_ARGx
			     &result,
			     yold - 1, screen_columns(SP_PARM) - 1,
			     ynew, xnew, ovw);
	break;
    }
#endif  

  nonlocal:
#if defined(MAIN) || defined(NCURSES_TEST)
    gettimeofday(&after, NULL);
    diff = after.tv_usec - before.tv_usec
	+ (after.tv_sec - before.tv_sec) * 1000000;
    if (!profiling)
	(void) fprintf(stderr,
		       "onscreen: %d microsec, %f 28.8Kbps char-equivalents\n",
		       (int) diff, diff / 288);
#endif  

    if (usecost != INFINITY) {
	TR(TRACE_MOVE, ("mvcur tactic %d", tactic));
	TPUTS_TRACE("mvcur");
	NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				buffer, 1, myOutCh);
	SP_PARM->_cursrow = ynew;
	SP_PARM->_curscol = xnew;
	return (OK);
    } else
	return (ERR);
}

 
static int
_nc_real_mvcur(NCURSES_SP_DCLx
	       int yold, int xold,
	       int ynew, int xnew,
	       NCURSES_SP_OUTC myOutCh,
	       int ovw)
{
    NCURSES_CH_T oldattr;
    int code;

    TR(TRACE_CALLS | TRACE_MOVE, (T_CALLED("_nc_real_mvcur(%p,%d,%d,%d,%d)"),
				  (void *) SP_PARM, yold, xold, ynew, xnew));

    if (SP_PARM == 0) {
	code = ERR;
    } else if (yold == ynew && xold == xnew) {
	code = OK;
    } else {

	 
	if (xnew >= screen_columns(SP_PARM)) {
	    ynew += xnew / screen_columns(SP_PARM);
	    xnew %= screen_columns(SP_PARM);
	}

	 
	oldattr = SCREEN_ATTRS(SP_PARM);
	if ((AttrOf(oldattr) & A_ALTCHARSET)
	    || (AttrOf(oldattr) && !move_standout_mode)) {
	    TR(TRACE_CHARPUT, ("turning off (%#lx) %s before move",
			       (unsigned long) AttrOf(oldattr),
			       _traceattr(AttrOf(oldattr))));
	    VIDPUTS(SP_PARM, A_NORMAL, 0);
	}

	if (xold >= screen_columns(SP_PARM)) {

	    int l = (xold + 1) / screen_columns(SP_PARM);

	    yold += l;
	    if (yold >= screen_lines(SP_PARM))
		l -= (yold - screen_lines(SP_PARM) - 1);

	    if (l > 0) {
		if (carriage_return) {
		    NCURSES_PUTP2("carriage_return", carriage_return);
		} else {
		    myOutCh(NCURSES_SP_ARGx '\r');
		}
		xold = 0;

		while (l > 0) {
		    if (newline) {
			NCURSES_PUTP2("newline", newline);
		    } else {
			myOutCh(NCURSES_SP_ARGx '\n');
		    }
		    l--;
		}
	    }
	}

	if (yold > screen_lines(SP_PARM) - 1)
	    yold = screen_lines(SP_PARM) - 1;
	if (ynew > screen_lines(SP_PARM) - 1)
	    ynew = screen_lines(SP_PARM) - 1;

	 
	code = onscreen_mvcur(NCURSES_SP_ARGx yold, xold, ynew, xnew, ovw, myOutCh);

	 
	if (!SameAttrOf(oldattr, SCREEN_ATTRS(SP_PARM))) {
	    TR(TRACE_CHARPUT, ("turning on (%#lx) %s after move",
			       (unsigned long) AttrOf(oldattr),
			       _traceattr(AttrOf(oldattr))));
	    VIDPUTS(SP_PARM, AttrOf(oldattr), GetPair(oldattr));
	}
    }
    returnCode(code);
}

 
NCURSES_EXPORT(int)
NCURSES_SP_NAME(_nc_mvcur) (NCURSES_SP_DCLx
			    int yold, int xold,
			    int ynew, int xnew)
{
    int rc;
    rc = _nc_real_mvcur(NCURSES_SP_ARGx yold, xold, ynew, xnew,
			NCURSES_SP_NAME(_nc_outch),
			TRUE);
     
    if ((SP_PARM != 0) && (SP_PARM->_endwin == ewInitial))
	NCURSES_SP_NAME(_nc_flush) (NCURSES_SP_ARG);
    return rc;
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
_nc_mvcur(int yold, int xold,
	  int ynew, int xnew)
{
    return NCURSES_SP_NAME(_nc_mvcur) (CURRENT_SCREEN, yold, xold, ynew, xnew);
}
#endif

#if defined(USE_TERM_DRIVER)
 
NCURSES_EXPORT(int)
TINFO_MVCUR(NCURSES_SP_DCLx int yold, int xold, int ynew, int xnew)
{
    int rc;
    rc = _nc_real_mvcur(NCURSES_SP_ARGx
			yold, xold,
			ynew, xnew,
			NCURSES_SP_NAME(_nc_outch),
			TRUE);
    if ((SP_PARM != 0) && (SP_PARM->_endwin == ewInitial))
	NCURSES_SP_NAME(_nc_flush) (NCURSES_SP_ARG);
    NCURSES_SP_NAME(_nc_flush) (NCURSES_SP_ARG);
    return rc;
}

#else  

 
NCURSES_EXPORT(int)
NCURSES_SP_NAME(mvcur) (NCURSES_SP_DCLx int yold, int xold, int ynew,
			int xnew)
{
    return _nc_real_mvcur(NCURSES_SP_ARGx
			  yold, xold,
			  ynew, xnew,
			  NCURSES_SP_NAME(_nc_putchar),
			  FALSE);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
mvcur(int yold, int xold, int ynew, int xnew)
{
    return NCURSES_SP_NAME(mvcur) (CURRENT_SCREEN, yold, xold, ynew, xnew);
}
#endif
#endif  

#if defined(TRACE) || defined(NCURSES_TEST)
NCURSES_EXPORT_VAR(int) _nc_optimize_enable = OPTIMIZE_ALL;
#endif

#if defined(MAIN) || defined(NCURSES_TEST)
 

#include <tic.h>
#include <dump_entry.h>
#include <time.h>

NCURSES_EXPORT_VAR(const char *) _nc_progname = "mvcur";

static unsigned long xmits;

 
NCURSES_EXPORT(int)
tputs(const char *string, int affcnt GCC_UNUSED, int (*outc) (int) GCC_UNUSED)
 
{
    if (profiling)
	xmits += strlen(string);
    else
	(void) fputs(_nc_visbuf(string), stdout);
    return (OK);
}

NCURSES_EXPORT(int)
putp(const char *string)
{
    return (tputs(string, 1, _nc_outch));
}

NCURSES_EXPORT(int)
_nc_outch(int ch)
{
    putc(ch, stdout);
    return OK;
}

NCURSES_EXPORT(int)
delay_output(int ms GCC_UNUSED)
{
    return OK;
}

static char tname[PATH_MAX];

static void
load_term(void)
{
    (void) setupterm(tname, STDOUT_FILENO, NULL);
}

static int
roll(int n)
{
    int i, j;

    i = (RAND_MAX / n) * n;
    while ((j = rand()) >= i)
	continue;
    return (j % n);
}

int
main(int argc GCC_UNUSED, char *argv[]GCC_UNUSED)
{
    _nc_STRCPY(tname, getenv("TERM"), sizeof(tname));
    load_term();
    _nc_setupscreen(lines, columns, stdout, FALSE, 0);
    baudrate();

    _nc_mvcur_init();

    (void) puts("The mvcur tester.  Type ? for help");

    fputs("smcup:", stdout);
    putchar('\n');

    for (;;) {
	int fy, fx, ty, tx, n, i;
	char buf[BUFSIZ], capname[BUFSIZ];

	if (fputs("> ", stdout) == EOF)
	    break;
	if (fgets(buf, sizeof(buf), stdin) == 0)
	    break;

#define PUTS(s)   (void) puts(s)
#define PUTF(s,t) (void) printf(s,t)
	if (buf[0] == '?') {
	    PUTS("?                -- display this help message");
	    PUTS("fy fx ty tx      -- (4 numbers) display (fy,fx)->(ty,tx) move");
	    PUTS("s[croll] n t b m -- display scrolling sequence");
	    PUTF("r[eload]         -- reload terminal info for %s\n",
		 termname());
	    PUTS("l[oad] <term>    -- load terminal info for type <term>");
	    PUTS("d[elete] <cap>   -- delete named capability");
	    PUTS("i[nspect]        -- display terminal capabilities");
	    PUTS("c[ost]           -- dump cursor-optimization cost table");
	    PUTS("o[optimize]      -- toggle movement optimization");
	    PUTS("t[orture] <num>  -- torture-test with <num> random moves");
	    PUTS("q[uit]           -- quit the program");
	} else if (sscanf(buf, "%d %d %d %d", &fy, &fx, &ty, &tx) == 4) {
	    struct timeval before, after;

	    putchar('"');

	    gettimeofday(&before, NULL);
	    mvcur(fy, fx, ty, tx);
	    gettimeofday(&after, NULL);

	    printf("\" (%ld msec)\n",
		   (long) (after.tv_usec - before.tv_usec
			   + (after.tv_sec - before.tv_sec)
			   * 1000000));
	} else if (sscanf(buf, "s %d %d %d %d", &fy, &fx, &ty, &tx) == 4) {
	    struct timeval before, after;

	    putchar('"');

	    gettimeofday(&before, NULL);
	    _nc_scrolln(fy, fx, ty, tx);
	    gettimeofday(&after, NULL);

	    printf("\" (%ld msec)\n",
		   (long) (after.tv_usec - before.tv_usec + (after.tv_sec -
							     before.tv_sec)
			   * 1000000));
	} else if (buf[0] == 'r') {
	    _nc_STRCPY(tname, termname(), sizeof(tname));
	    load_term();
	} else if (sscanf(buf, "l %s", tname) == 1) {
	    load_term();
	} else if (sscanf(buf, "d %s", capname) == 1) {
	    struct name_table_entry const *np = _nc_find_entry(capname,
							       _nc_get_hash_table(FALSE));

	    if (np == NULL)
		(void) printf("No such capability as \"%s\"\n", capname);
	    else {
		switch (np->nte_type) {
		case BOOLEAN:
		    cur_term->type.Booleans[np->nte_index] = FALSE;
		    (void)
			printf("Boolean capability `%s' (%d) turned off.\n",
			       np->nte_name, np->nte_index);
		    break;

		case NUMBER:
		    cur_term->type.Numbers[np->nte_index] = ABSENT_NUMERIC;
		    (void) printf("Number capability `%s' (%d) set to -1.\n",
				  np->nte_name, np->nte_index);
		    break;

		case STRING:
		    cur_term->type.Strings[np->nte_index] = ABSENT_STRING;
		    (void) printf("String capability `%s' (%d) deleted.\n",
				  np->nte_name, np->nte_index);
		    break;
		}
	    }
	} else if (buf[0] == 'i') {
	    dump_init(NULL, F_TERMINFO, S_TERMINFO,
		      FALSE, 70, 0, 0, FALSE, FALSE, 0);
	    dump_entry(&TerminalType(cur_term), FALSE, TRUE, 0, 0);
	    putchar('\n');
	} else if (buf[0] == 'o') {
	    if (_nc_optimize_enable & OPTIMIZE_MVCUR) {
		_nc_optimize_enable &= ~OPTIMIZE_MVCUR;
		(void) puts("Optimization is now off.");
	    } else {
		_nc_optimize_enable |= OPTIMIZE_MVCUR;
		(void) puts("Optimization is now on.");
	    }
	}
	 
	else if (sscanf(buf, "t %d", &n) == 1) {
	    float cumtime = 0.0, perchar;
	    int speeds[] =
	    {2400, 9600, 14400, 19200, 28800, 38400, 0};

	    srand((unsigned) (getpid() + time((time_t *) 0)));
	    profiling = TRUE;
	    xmits = 0;
	    for (i = 0; i < n; i++) {
		 
#ifdef FIND_COREDUMP
		int from_y = roll(lines);
		int to_y = roll(lines);
		int from_x = roll(columns);
		int to_x = roll(columns);

		printf("(%d,%d) -> (%d,%d)\n", from_y, from_x, to_y, to_x);
		mvcur(from_y, from_x, to_y, to_x);
#else
		mvcur(roll(lines), roll(columns), roll(lines), roll(columns));
#endif  
		if (diff)
		    cumtime += diff;
	    }
	    profiling = FALSE;

	     
	    perchar = cumtime / n;

	    (void) printf("%d moves (%ld chars) in %d msec, %f msec each:\n",
			  n, xmits, (int) cumtime, perchar);

	    for (i = 0; speeds[i]; i++) {
		 
		float totalest = cumtime + xmits * 9 * 1e6 / speeds[i];

		 
		float overhead = speeds[i] * perchar / 1e6;

		(void)
		    printf("%6d bps: %3.2f char-xmits overhead; total estimated time %15.2f\n",
			   speeds[i], overhead, totalest);
	    }
	} else if (buf[0] == 'c') {
	    (void) printf("char padding: %d\n", CURRENT_SCREEN->_char_padding);
	    (void) printf("cr cost: %d\n", CURRENT_SCREEN->_cr_cost);
	    (void) printf("cup cost: %d\n", CURRENT_SCREEN->_cup_cost);
	    (void) printf("home cost: %d\n", CURRENT_SCREEN->_home_cost);
	    (void) printf("ll cost: %d\n", CURRENT_SCREEN->_ll_cost);
#if USE_HARD_TABS
	    (void) printf("ht cost: %d\n", CURRENT_SCREEN->_ht_cost);
	    (void) printf("cbt cost: %d\n", CURRENT_SCREEN->_cbt_cost);
#endif  
	    (void) printf("cub1 cost: %d\n", CURRENT_SCREEN->_cub1_cost);
	    (void) printf("cuf1 cost: %d\n", CURRENT_SCREEN->_cuf1_cost);
	    (void) printf("cud1 cost: %d\n", CURRENT_SCREEN->_cud1_cost);
	    (void) printf("cuu1 cost: %d\n", CURRENT_SCREEN->_cuu1_cost);
	    (void) printf("cub cost: %d\n", CURRENT_SCREEN->_cub_cost);
	    (void) printf("cuf cost: %d\n", CURRENT_SCREEN->_cuf_cost);
	    (void) printf("cud cost: %d\n", CURRENT_SCREEN->_cud_cost);
	    (void) printf("cuu cost: %d\n", CURRENT_SCREEN->_cuu_cost);
	    (void) printf("hpa cost: %d\n", CURRENT_SCREEN->_hpa_cost);
	    (void) printf("vpa cost: %d\n", CURRENT_SCREEN->_vpa_cost);
	} else if (buf[0] == 'x' || buf[0] == 'q')
	    break;
	else
	    (void) puts("Invalid command.");
    }

    (void) fputs("rmcup:", stdout);
    _nc_mvcur_wrap();
    putchar('\n');

    return (0);
}

#endif  

 
