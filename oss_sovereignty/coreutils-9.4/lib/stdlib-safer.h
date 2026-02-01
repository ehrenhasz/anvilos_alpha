 

int mkstemp_safer (char *);

#if GNULIB_MKOSTEMP
int mkostemp_safer (char *, int);
#endif

#if GNULIB_MKOSTEMPS
int mkostemps_safer (char *, int, int);
#endif

#if GNULIB_MKSTEMPS
int mkstemps_safer (char *, int);
#endif
