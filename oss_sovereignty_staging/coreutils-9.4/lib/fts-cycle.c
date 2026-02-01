 
struct Active_dir
{
  dev_t dev;
  ino_t ino;
  FTSENT *fts_ent;
};

static bool
AD_compare (void const *x, void const *y)
{
  struct Active_dir const *ax = x;
  struct Active_dir const *ay = y;
  return ax->ino == ay->ino
      && ax->dev == ay->dev;
}

static size_t
AD_hash (void const *x, size_t table_size)
{
  struct Active_dir const *ax = x;
  return (uintmax_t) ax->ino % table_size;
}

 

static bool
setup_dir (FTS *fts)
{
  if (fts->fts_options & (FTS_TIGHT_CYCLE_CHECK | FTS_LOGICAL))
    {
      enum { HT_INITIAL_SIZE = 31 };
      fts->fts_cycle.ht = hash_initialize (HT_INITIAL_SIZE, NULL, AD_hash,
                                           AD_compare, free);
      if (! fts->fts_cycle.ht)
        return false;
    }
  else
    {
      fts->fts_cycle.state = malloc (sizeof *fts->fts_cycle.state);
      if (! fts->fts_cycle.state)
        return false;
      cycle_check_init (fts->fts_cycle.state);
    }

  return true;
}

 

static bool
enter_dir (FTS *fts, FTSENT *ent)
{
  if (fts->fts_options & (FTS_TIGHT_CYCLE_CHECK | FTS_LOGICAL))
    {
      struct stat const *st = ent->fts_statp;
      struct Active_dir *ad = malloc (sizeof *ad);
      struct Active_dir *ad_from_table;

      if (!ad)
        return false;

      ad->dev = st->st_dev;
      ad->ino = st->st_ino;
      ad->fts_ent = ent;

       
      ad_from_table = hash_insert (fts->fts_cycle.ht, ad);

      if (ad_from_table != ad)
        {
          free (ad);
          if (!ad_from_table)
            return false;

           
          ent->fts_cycle = ad_from_table->fts_ent;
          ent->fts_info = FTS_DC;
        }
    }
  else
    {
      if (cycle_check (fts->fts_cycle.state, ent->fts_statp))
        {
           
          ent->fts_cycle = ent;
          ent->fts_info = FTS_DC;
        }
    }

  return true;
}

 

static void
leave_dir (FTS *fts, FTSENT *ent)
{
  struct stat const *st = ent->fts_statp;
  if (fts->fts_options & (FTS_TIGHT_CYCLE_CHECK | FTS_LOGICAL))
    {
      struct Active_dir obj;
      void *found;
      obj.dev = st->st_dev;
      obj.ino = st->st_ino;
      found = hash_remove (fts->fts_cycle.ht, &obj);
      if (!found)
        abort ();
      free (found);
    }
  else
    {
      FTSENT *parent = ent->fts_parent;
      if (parent != NULL && 0 <= parent->fts_level)
        CYCLE_CHECK_REFLECT_CHDIR_UP (fts->fts_cycle.state,
                                      *(parent->fts_statp), *st);
    }
}

 

static void
free_dir (FTS *sp)
{
  if (sp->fts_options & (FTS_TIGHT_CYCLE_CHECK | FTS_LOGICAL))
    {
      if (sp->fts_cycle.ht)
        hash_free (sp->fts_cycle.ht);
    }
  else
    free (sp->fts_cycle.state);
}
