 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#include <stdio.h>	 

#include "readline.h"
#include "rlconf.h"

#include "emacs_keymap.c"

#if defined (VI_MODE)
#include "vi_keymap.c"
#endif

#include "xmalloc.h"

 
 
 
 
 


 
Keymap
rl_make_bare_keymap (void)
{
  register int i;
  Keymap keymap;

  keymap = (Keymap)xmalloc (KEYMAP_SIZE * sizeof (KEYMAP_ENTRY));
  for (i = 0; i < KEYMAP_SIZE; i++)
    {
      keymap[i].type = ISFUNC;
      keymap[i].function = (rl_command_func_t *)NULL;
    }

#if 0
  for (i = 'A'; i < ('Z' + 1); i++)
    {
      keymap[i].type = ISFUNC;
      keymap[i].function = rl_do_lowercase_version;
    }
#endif

  return (keymap);
}

 
int
rl_empty_keymap (Keymap keymap)
{
  int i;

  for (i = 0; i < ANYOTHERKEY; i++)
    {
      if (keymap[i].type != ISFUNC || keymap[i].function)
	return 0;
    }
  return 1;
}

 
Keymap
rl_copy_keymap (Keymap map)
{
  register int i;
  Keymap temp;

  temp = rl_make_bare_keymap ();
  for (i = 0; i < KEYMAP_SIZE; i++)
    {
      temp[i].type = map[i].type;
      temp[i].function = map[i].function;
    }
  return (temp);
}

 
Keymap
rl_make_keymap (void)
{
  register int i;
  Keymap newmap;

  newmap = rl_make_bare_keymap ();

   
  for (i = ' '; i < 127; i++)
    newmap[i].function = rl_insert;

  newmap[TAB].function = rl_insert;
  newmap[RUBOUT].function = rl_rubout;	 
  newmap[CTRL('H')].function = rl_rubout;

#if KEYMAP_SIZE > 128
   
  for (i = 128; i < 256; i++)
    newmap[i].function = rl_insert;
#endif  

  return (newmap);
}

 
void
rl_discard_keymap (Keymap map)
{
  int i;

  if (map == 0)
    return;

  for (i = 0; i < KEYMAP_SIZE; i++)
    {
      switch (map[i].type)
	{
	case ISFUNC:
	  break;

	case ISKMAP:
	  rl_discard_keymap ((Keymap)map[i].function);
	  xfree ((char *)map[i].function);
	  break;

	case ISMACR:
	  xfree ((char *)map[i].function);
	  break;
	}
    }
}

 
void
rl_free_keymap (Keymap map)
{
  rl_discard_keymap (map);
  xfree ((char *)map);
}
