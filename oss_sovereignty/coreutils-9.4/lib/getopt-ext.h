 

__BEGIN_DECLS

 

struct option
{
  const char *name;
   
  int has_arg;
  int *flag;
  int val;
};

 

#define no_argument		0
#define required_argument	1
#define optional_argument	2

extern int getopt_long (int ___argc, char *__getopt_argv_const *___argv,
			const char *__shortopts,
		        const struct option *__longopts, int *__longind)
       __THROW _GL_ARG_NONNULL ((2, 3));
extern int getopt_long_only (int ___argc, char *__getopt_argv_const *___argv,
			     const char *__shortopts,
		             const struct option *__longopts, int *__longind)
       __THROW _GL_ARG_NONNULL ((2, 3));

__END_DECLS

#endif  
