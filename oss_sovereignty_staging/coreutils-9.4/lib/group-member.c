 
#include <unistd.h>

#include <stdckdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

 
enum { GROUPBUF_SIZE = 100 };

struct group_info
  {
    gid_t *group;
    gid_t groupbuf[GROUPBUF_SIZE];
  };

static void
free_group_info (struct group_info const *g)
{
  if (g->group != g->groupbuf)
    free (g->group);
}

static int
get_group_info (struct group_info *gi)
{
  int n_groups = getgroups (GROUPBUF_SIZE, gi->groupbuf);
  gi->group = gi->groupbuf;

  if (n_groups < 0)
    {
      int n_group_slots = getgroups (0, NULL);
      size_t nbytes;
      if (! ckd_mul (&nbytes, n_group_slots, sizeof *gi->group))
        {
          gi->group = malloc (nbytes);
          if (gi->group)
            n_groups = getgroups (n_group_slots, gi->group);
        }
    }

   
  return n_groups;
}

 

int
group_member (gid_t gid)
{
  int i;
  int found;
  struct group_info gi;
  int n_groups = get_group_info (&gi);

   
  found = 0;
  for (i = 0; i < n_groups; i++)
    {
      if (gid == gi.group[i])
        {
          found = 1;
          break;
        }
    }

  free_group_info (&gi);

  return found;
}

#ifdef TEST

int
main (int argc, char **argv)
{
  int i;

  for (i = 1; i < argc; i++)
    {
      gid_t gid;

      gid = atoi (argv[i]);
      printf ("%d: %s\n", gid, group_member (gid) ? "yes" : "no");
    }
  exit (0);
}

#endif  
