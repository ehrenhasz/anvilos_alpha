 
  for (n = 1; n <= NMAX; n++)
    {
      struct foo *dst;
      struct foo *tmp;
      double *qsort_result;
      size_t i;

      dst = (struct foo *) malloc ((n + 1) * sizeof (struct foo));
      dst[n].x = 0x4A6A71FE;  
      tmp = (struct foo *) malloc ((n / 2 + 1) * sizeof (struct foo));
      tmp[n / 2].x = 0x587EF149;  

      merge_sort_fromto (data, dst, n, tmp);

       
      ASSERT (dst[n].x == 0x4A6A71FE);
      ASSERT (tmp[n / 2].x == 0x587EF149);

       
      qsort_result = (double *) malloc (n * sizeof (double));
      for (i = 0; i < n; i++)
        qsort_result[i] = data[i].x;
      qsort (qsort_result, n, sizeof (double), cmp_double);
      for (i = 0; i < n; i++)
        ASSERT (dst[i].x == qsort_result[i]);

       
      for (i = 0; i < n; i++)
        if (i > 0 && dst[i - 1].x == dst[i].x)
          ASSERT (dst[i - 1].index < dst[i].index);

      free (qsort_result);
      free (tmp);
      free (dst);
    }

   
  for (n = 1; n <= NMAX; n++)
    {
      struct foo *src;
      struct foo *tmp;
      double *qsort_result;
      size_t i;

      src = (struct foo *) malloc ((n + 1) * sizeof (struct foo));
      src[n].x = 0x4A6A71FE;  
      tmp = (struct foo *) malloc ((n + 1) * sizeof (struct foo));
      tmp[n].x = 0x587EF149;  

      for (i = 0; i < n; i++)
        src[i] = data[i];

      merge_sort_inplace (src, n, tmp);

       
      ASSERT (src[n].x == 0x4A6A71FE);
      ASSERT (tmp[n].x == 0x587EF149);

       
      qsort_result = (double *) malloc (n * sizeof (double));
      for (i = 0; i < n; i++)
        qsort_result[i] = data[i].x;
      qsort (qsort_result, n, sizeof (double), cmp_double);
      for (i = 0; i < n; i++)
        ASSERT (src[i].x == qsort_result[i]);

       
      for (i = 0; i < n; i++)
        if (i > 0 && src[i - 1].x == src[i].x)
          ASSERT (src[i - 1].index < src[i].index);

      free (qsort_result);
      free (tmp);
      free (src);
    }

  return 0;
}
