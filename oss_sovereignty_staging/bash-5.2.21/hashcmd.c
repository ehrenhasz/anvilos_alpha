 

 

#include <config.h>

#include "bashtypes.h"
#include "posixstat.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "bashansi.h"

#include "shell.h"
#include "flags.h"
#include "findcmd.h"
#include "hashcmd.h"

HASH_TABLE *hashed_filenames = (HASH_TABLE *)NULL;

static void phash_freedata PARAMS((PTR_T));

void
phash_create ()
{
  if (hashed_filenames == 0)
    hashed_filenames = hash_create (FILENAME_HASH_BUCKETS);
}

static void
phash_freedata (data)
     PTR_T data;
{
  free (((PATH_DATA *)data)->path);
  free (data);
}

void
phash_flush ()
{
  if (hashed_filenames)
    hash_flush (hashed_filenames, phash_freedata);
}

 
int
phash_remove (filename)
     const char *filename;
{
  register BUCKET_CONTENTS *item;

  if (hashing_enabled == 0 || hashed_filenames == 0)
    return 0;

  item = hash_remove (filename, hashed_filenames, 0);
  if (item)
    {
      if (item->data)
	phash_freedata (item->data);
      free (item->key);
      free (item);
      return 0;
    }
  return 1;
}

 
void
phash_insert (filename, full_path, check_dot, found)
     char *filename, *full_path;
     int check_dot, found;
{
  register BUCKET_CONTENTS *item;

  if (hashing_enabled == 0)
    return;

  if (hashed_filenames == 0)
    phash_create ();

  item = hash_insert (filename, hashed_filenames, 0);
  if (item->data)
    free (pathdata(item)->path);
  else
    {
      item->key = savestring (filename);
      item->data = xmalloc (sizeof (PATH_DATA));
    }
  pathdata(item)->path = savestring (full_path);
  pathdata(item)->flags = 0;
  if (check_dot)
    pathdata(item)->flags |= HASH_CHKDOT;
  if (*full_path != '/')
    pathdata(item)->flags |= HASH_RELPATH;
  item->times_found = found;
}

 
char *
phash_search (filename)
     const char *filename;
{
  register BUCKET_CONTENTS *item;
  char *path, *dotted_filename, *tail;
  int same;

  if (hashing_enabled == 0 || hashed_filenames == 0)
    return ((char *)NULL);

  item = hash_search (filename, hashed_filenames, 0);

  if (item == NULL)
    return ((char *)NULL);

   
  path = pathdata(item)->path;
  if (pathdata(item)->flags & (HASH_CHKDOT|HASH_RELPATH))
    {
      tail = (pathdata(item)->flags & HASH_RELPATH) ? path : (char *)filename;	 
       
      if (tail[0] != '.' || tail[1] != '/')
	{
	  dotted_filename = (char *)xmalloc (3 + strlen (tail));
	  dotted_filename[0] = '.'; dotted_filename[1] = '/';
	  strcpy (dotted_filename + 2, tail);
	}
      else
	dotted_filename = savestring (tail);

      if (executable_file (dotted_filename))
	return (dotted_filename);

      free (dotted_filename);

#if 0
      if (pathdata(item)->flags & HASH_RELPATH)
	return ((char *)NULL);
#endif

       

       
      if (*path == '.')
	{
	  same = 0;
	  tail = (char *)strrchr (path, '/');

	  if (tail)
	    {
	      *tail = '\0';
	      same = same_file (".", path, (struct stat *)NULL, (struct stat *)NULL);
	      *tail = '/';
	    }

	  return same ? (char *)NULL : savestring (path);
	}
    }

  return (savestring (path));
}
