 
#if defined __MVS__ && defined __IBMC__ && !defined __BFP__
# error "NaN is not supported with IBM's hexadecimal floating-point format; please re-compile with -qfloat=ieee"
#endif

 

 
#if (defined __DECC || defined _MSC_VER \
     || (defined __MVS__ && defined __IBMC__)   \
     || defined __PGI)
static float
NaNf ()
{
  static float zero = 0.0f;
  return zero / zero;
}
#else
# define NaNf() (0.0f / 0.0f)
#endif


 

 
#if (defined __DECC || defined _MSC_VER \
     || (defined __MVS__ && defined __IBMC__)   \
     || defined __PGI)
static double
NaNd ()
{
  static double zero = 0.0;
  return zero / zero;
}
#else
# define NaNd() (0.0 / 0.0)
#endif


 

 
#ifdef __sgi
static long double NaNl ()
{
  double zero = 0.0;
  return zero / zero;
}
#elif defined _MSC_VER || (defined __MVS__ && defined __IBMC__) || defined __PGI
static long double
NaNl ()
{
  static long double zero = 0.0L;
  return zero / zero;
}
#else
# define NaNl() (0.0L / 0.0L)
#endif
