 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

_GL_INLINE_HEADER_BEGIN
#ifndef PRIV_SET_INLINE
# define PRIV_SET_INLINE _GL_INLINE
#endif

#if HAVE_GETPPRIV && HAVE_PRIV_H

# include <priv.h>

int priv_set_ismember (const char *priv);
int priv_set_remove (const char *priv);
int priv_set_restore (const char *priv);

PRIV_SET_INLINE int
priv_set_remove_linkdir (void)
{
  return priv_set_remove (PRIV_SYS_LINKDIR);
}

PRIV_SET_INLINE int
priv_set_restore_linkdir (void)
{
  return priv_set_restore (PRIV_SYS_LINKDIR);
}

#else

PRIV_SET_INLINE int
priv_set_remove_linkdir (void)
{
  return -1;
}

PRIV_SET_INLINE int
priv_set_restore_linkdir (void)
{
  return -1;
}

#endif

_GL_INLINE_HEADER_END
