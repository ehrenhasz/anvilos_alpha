 

#ifndef _GL_SAVEDIR_H
#define _GL_SAVEDIR_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <dirent.h>
#include <stdlib.h>

enum savedir_option
  {
    SAVEDIR_SORT_NONE,
    SAVEDIR_SORT_NAME,
#if D_INO_IN_DIRENT
    SAVEDIR_SORT_INODE,
    SAVEDIR_SORT_FASTREAD = SAVEDIR_SORT_INODE
#else
    SAVEDIR_SORT_FASTREAD = SAVEDIR_SORT_NONE
#endif
  };

char *streamsavedir (DIR *, enum savedir_option)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;
char *savedir (char const *, enum savedir_option)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;

#endif
