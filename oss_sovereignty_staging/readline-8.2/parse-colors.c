 

 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <stdio.h>

 
#if defined (HAVE_STRING_H)
#  include <string.h>
#else  
#  include <strings.h>
#endif  

 
#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#include "rldefs.h"	 
#include "readline.h"
#include "rlprivate.h"
#include "rlshell.h"
#include "xmalloc.h"

#include "colors.h"
#include "parse-colors.h"

#if defined (COLOR_SUPPORT)

static bool get_funky_string (char **dest, const char **src, bool equals_end, size_t *output_count);

struct bin_str _rl_color_indicator[] =
  {
    { LEN_STR_PAIR ("\033[") },          
    { LEN_STR_PAIR ("m") },              
    { 0, NULL },                         
    { LEN_STR_PAIR ("0") },              
    { 0, NULL },                         
    { 0, NULL },                         
    { LEN_STR_PAIR ("01;34") },          
    { LEN_STR_PAIR ("01;36") },          
    { LEN_STR_PAIR ("33") },             
    { LEN_STR_PAIR ("01;35") },          
    { LEN_STR_PAIR ("01;33") },          
    { LEN_STR_PAIR ("01;33") },          
    { 0, NULL },                         
    { 0, NULL },                         
    { LEN_STR_PAIR ("01;32") },          
    { LEN_STR_PAIR ("01;35") },          
    { LEN_STR_PAIR ("37;41") },          
    { LEN_STR_PAIR ("30;43") },          
    { LEN_STR_PAIR ("37;44") },          
    { LEN_STR_PAIR ("34;42") },          
    { LEN_STR_PAIR ("30;42") },          
    { LEN_STR_PAIR ("30;41") },          
    { 0, NULL },                         
    { LEN_STR_PAIR ("\033[K") },         
  };

 

static bool
get_funky_string (char **dest, const char **src, bool equals_end, size_t *output_count) {
  char num;			 
  size_t count;			 
  enum {
    ST_GND, ST_BACKSLASH, ST_OCTAL, ST_HEX, ST_CARET, ST_END, ST_ERROR
  } state;
  const char *p;
  char *q;

  p = *src;			 
  q = *dest;			 

  count = 0;			 
  num = 0;

  state = ST_GND;		 
  while (state < ST_END)
    {
      switch (state)
        {
        case ST_GND:		 
          switch (*p)
            {
            case ':':
            case '\0':
              state = ST_END;	 
              break;
            case '\\':
              state = ST_BACKSLASH;  
              ++p;
              break;
            case '^':
              state = ST_CARET;  
              ++p;
              break;
            case '=':
              if (equals_end)
                {
                  state = ST_END;  
                  break;
                }
               
            default:
              *(q++) = *(p++);
              ++count;
              break;
            }
          break;

        case ST_BACKSLASH:	 
          switch (*p)
            {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
              state = ST_OCTAL;	 
              num = *p - '0';
              break;
            case 'x':
            case 'X':
              state = ST_HEX;	 
              num = 0;
              break;
            case 'a':		 
              num = '\a';
              break;
            case 'b':		 
              num = '\b';
              break;
            case 'e':		 
              num = 27;
              break;
            case 'f':		 
              num = '\f';
              break;
            case 'n':		 
              num = '\n';
              break;
            case 'r':		 
              num = '\r';
              break;
            case 't':		 
              num = '\t';
              break;
            case 'v':		 
              num = '\v';
              break;
            case '?':		 
              num = 127;
              break;
            case '_':		 
              num = ' ';
              break;
            case '\0':		 
              state = ST_ERROR;	 
              break;
            default:		 
              num = *p;
              break;
            }
          if (state == ST_BACKSLASH)
            {
              *(q++) = num;
              ++count;
              state = ST_GND;
            }
          ++p;
          break;

        case ST_OCTAL:		 
          if (*p < '0' || *p > '7')
            {
              *(q++) = num;
              ++count;
              state = ST_GND;
            }
          else
            num = (num << 3) + (*(p++) - '0');
          break;

        case ST_HEX:		 
          switch (*p)
            {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
              num = (num << 4) + (*(p++) - '0');
              break;
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
              num = (num << 4) + (*(p++) - 'a') + 10;
              break;
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
              num = (num << 4) + (*(p++) - 'A') + 10;
              break;
            default:
              *(q++) = num;
              ++count;
              state = ST_GND;
              break;
            }
          break;

        case ST_CARET:		 
          state = ST_GND;	 
          if (*p >= '@' && *p <= '~')
            {
              *(q++) = *(p++) & 037;
              ++count;
            }
          else if (*p == '?')
            {
              *(q++) = 127;
              ++count;
            }
          else
            state = ST_ERROR;
          break;

        default:
	   
           
          state = ST_ERROR;
          break;
        }
    }

  *dest = q;
  *src = p;
  *output_count = count;

  return state != ST_ERROR;
}
#endif  

void _rl_parse_colors(void)
{
#if defined (COLOR_SUPPORT)
  const char *p;		 
  char *buf;			 
  int state;			 
  int ind_no;			 
  char label[3];		 
  COLOR_EXT_TYPE *ext;		 

  p = sh_get_env_value ("LS_COLORS");
  if (p == 0 || *p == '\0')
    {
      _rl_color_ext_list = NULL;
      return;
    }

  ext = NULL;
  strcpy (label, "??");

   
  buf = color_buf = savestring (p);

  state = 1;
  while (state > 0)
    {
      switch (state)
        {
        case 1:		 
          switch (*p)
            {
            case ':':
              ++p;
              break;

            case '*':
               

              ext = (COLOR_EXT_TYPE *)xmalloc (sizeof *ext);
              ext->next = _rl_color_ext_list;
              _rl_color_ext_list = ext;

              ++p;
              ext->ext.string = buf;

              state = (get_funky_string (&buf, &p, true, &ext->ext.len)
                       ? 4 : -1);
              break;

            case '\0':
              state = 0;	 
              break;

            default:	 
              label[0] = *(p++);
              state = 2;
              break;
            }
          break;

        case 2:		 
          if (*p)
            {
              label[1] = *(p++);
              state = 3;
            }
          else
            state = -1;	 
          break;

        case 3:		 
          state = -1;	 
          if (*(p++) == '=') 
            {
              for (ind_no = 0; indicator_name[ind_no] != NULL; ++ind_no)
                {
                  if (STREQ (label, indicator_name[ind_no]))
                    {
                      _rl_color_indicator[ind_no].string = buf;
                      state = (get_funky_string (&buf, &p, false,
                                                 &_rl_color_indicator[ind_no].len)
                               ? 1 : -1);
                      break;
                    }
                }
              if (state == -1)
		{
                  _rl_errmsg ("LS_COLORS: unrecognized prefix: %s", label);
                   
                  while (p && *p && *p != ':')
		    p++;
		  if (p && *p == ':')
		    state = 1;
		  else if (p && *p == 0)
		    state = 0;
		}
            }
          break;

        case 4:		 
          if (*(p++) == '=')
            {
              ext->seq.string = buf;
              state = (get_funky_string (&buf, &p, false, &ext->seq.len)
                       ? 1 : -1);
            }
          else
            state = -1;
           
          if (state == -1 && ext->ext.string)
	    _rl_errmsg ("LS_COLORS: syntax error: %s", ext->ext.string);
          break;
        }
    }

  if (state < 0)
    {
      COLOR_EXT_TYPE *e;
      COLOR_EXT_TYPE *e2;

      _rl_errmsg ("unparsable value for LS_COLORS environment variable");
      free (color_buf);
      for (e = _rl_color_ext_list; e != NULL;  )
        {
          e2 = e;
          e = e->next;
          free (e2);
        }
      _rl_color_ext_list = NULL;
      _rl_colored_stats = 0;	 
    }
#else  
  ;
#endif  
}
