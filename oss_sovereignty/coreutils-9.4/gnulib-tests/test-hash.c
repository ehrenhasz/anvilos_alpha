 
      hash_clear (ht);
      hash_free (ht);

      ht = hash_initialize (sz, NULL, hash_pjw, hash_compare_strings, NULL);
      ASSERT (ht);

      insert_new (ht, "z");
      insert_new (ht, "y");
      insert_new (ht, "x");
      insert_new (ht, "w");
      insert_new (ht, "v");
      insert_new (ht, "u");

      hash_clear (ht);
      ASSERT (hash_get_n_entries (ht) == 0);
      hash_free (ht);

       
      ht = hash_initialize (sz, NULL, NULL, NULL, NULL);
      ASSERT (ht);
      {
        char *str = strdup ("a");
        ASSERT (str);
        insert_new (ht, "a");
        insert_new (ht, str);
        ASSERT (hash_lookup (ht, str) == str);
        free (str);
      }
      hash_free (ht);
    }

  hash_reset_tuning (&tuning);
  tuning.shrink_threshold = 0.3;
  tuning.shrink_factor = 0.707;
  tuning.growth_threshold = 1.5;
  tuning.growth_factor = 2.0;
  tuning.is_n_buckets = true;
   
  ht = hash_initialize (4651, &tuning, hash_pjw, hash_compare_strings,
                        hash_freer);
  ASSERT (!ht);

   
  tuning.growth_threshold = 0.89;

   
  for (k = 0; k < 2; k++)
    {
      Hash_tuning const *tune = (k == 0 ? NULL : &tuning);
       
      ht = hash_initialize (4651, tune, hash_pjw,
                            hash_compare_strings, hash_freer);
      ASSERT (ht);
      for (i = 0; i < 10000; i++)
        {
          unsigned int op = rand () % 10;
          switch (op)
            {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
              {
                char buf[50];
                char const *p = uinttostr (i, buf);
                char *p_dup = strdup (p);
                ASSERT (p_dup);
                insert_new (ht, p_dup);
              }
              break;

            case 6:
              {
                size_t n = hash_get_n_entries (ht);
                ASSERT (hash_rehash (ht, n + rand () % 20));
              }
              break;

            case 7:
              {
                size_t n = hash_get_n_entries (ht);
                size_t delta = rand () % 20;
                if (delta < n)
                  ASSERT (hash_rehash (ht, n - delta));
              }
              break;

            case 8:
            case 9:
              {
                 
                size_t n = hash_get_n_entries (ht);
                if (n)
                  {
                    size_t kk = rand () % n;
                    void const *p;
                    void *v;
                    for (p = hash_get_first (ht); kk;
                         --kk, p = hash_get_next (ht, p))
                      {
                         
                      }
                    ASSERT (p);
                    v = hash_remove (ht, p);
                    ASSERT (v);
                    free (v);
                  }
                break;
              }
            }
          ASSERT (hash_table_ok (ht));
        }

      hash_free (ht);
    }

  return 0;
}
