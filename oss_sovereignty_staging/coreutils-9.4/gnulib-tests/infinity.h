 

 
#if defined _MSC_VER || (defined __MVS__ && defined __IBMC__) || defined __PGI
static float
Infinityf ()
{
  static float zero = 0.0f;
  return 1.0f / zero;
}
#else
# define Infinityf() (1.0f / 0.0f)
#endif


 

 
#if defined _MSC_VER || (defined __MVS__ && defined __IBMC__) || defined __PGI
static double
Infinityd ()
{
  static double zero = 0.0;
  return 1.0 / zero;
}
#else
# define Infinityd() (1.0 / 0.0)
#endif


 

 
#if defined _MSC_VER || (defined __MVS__ && defined __IBMC__) || defined __PGI
static long double
Infinityl ()
{
  static long double zero = 0.0L;
  return 1.0L / zero;
}
#else
# define Infinityl() (1.0L / 0.0L)
#endif
