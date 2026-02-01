 

 

#ifndef __C_VARARGS_TO_ADA_H
#define __C_VARARGS_TO_ADA_H

#ifdef HAVE_CONFIG_H
#include <ncurses_cfg.h>
#else
#include <ncurses.h>
#endif

#include <stdlib.h>

#include <form.h>

extern int set_field_type_alnum(FIELD *   ,
				int   );

extern int set_field_type_alpha(FIELD *   ,
				int   );

extern int set_field_type_enum(FIELD *   ,
			       char **   ,
			       int   ,
			       int   );

extern int set_field_type_integer(FIELD *   ,
				  int   ,
				  long   ,
				  long   );

extern int set_field_type_numeric(FIELD *   ,
				  int   ,
				  double   ,
				  double   );

extern int set_field_type_regexp(FIELD *   ,
				 char *   );

extern int set_field_type_ipv4(FIELD *   );

extern int set_field_type_user(FIELD *   ,
			       FIELDTYPE *   ,
			       void *   );

extern void *void_star_make_arg(va_list *   );

#ifdef TRACE
extern void _traces(const char *	 
		    ,char *   );
#endif

#endif  
