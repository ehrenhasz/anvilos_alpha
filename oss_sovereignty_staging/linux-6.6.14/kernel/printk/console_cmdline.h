 
#ifndef _CONSOLE_CMDLINE_H
#define _CONSOLE_CMDLINE_H

struct console_cmdline
{
	char	name[16];			 
	int	index;				 
	bool	user_specified;			 
	char	*options;			 
#ifdef CONFIG_A11Y_BRAILLE_CONSOLE
	char	*brl_options;			 
#endif
};

#endif
