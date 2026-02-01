 

 

 

     
{
    char *strp;
    short capval;

#define EXTRACT_DELAY(str) \
    	(short) (strp = strchr(str, '*'), strp ? atoi(strp+1) : 0)

     
    if (VALID_STRING(carriage_return)
	&& (capval = EXTRACT_DELAY(carriage_return)))
	carriage_return_delay = capval;
    if (VALID_STRING(newline) && (capval = EXTRACT_DELAY(newline)))
	new_line_delay = capval;

     
    if (!VALID_STRING(termcap_init2) && VALID_STRING(init_3string)) {
	termcap_init2 = init_3string;
	init_3string = ABSENT_STRING;
    }
    if (!VALID_STRING(termcap_reset)
     && VALID_STRING(reset_2string)
     && !VALID_STRING(reset_1string)
     && !VALID_STRING(reset_3string)) {
	termcap_reset = reset_2string;
	reset_2string = ABSENT_STRING;
    }
    if (magic_cookie_glitch_ul == ABSENT_NUMERIC
	&& magic_cookie_glitch != ABSENT_NUMERIC
	&& VALID_STRING(enter_underline_mode))
	magic_cookie_glitch_ul = magic_cookie_glitch;

     
    linefeed_is_newline = (char) (VALID_STRING(newline)
				  && (strcmp("\n", newline) == 0));
    if (VALID_STRING(cursor_left)
	&& (capval = EXTRACT_DELAY(cursor_left)))
	backspace_delay = capval;
    if (VALID_STRING(tab) && (capval = EXTRACT_DELAY(tab)))
	horizontal_tab_delay = capval;
#undef EXTRACT_DELAY
}
