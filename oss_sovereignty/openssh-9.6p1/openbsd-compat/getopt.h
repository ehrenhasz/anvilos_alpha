 
 

 

#ifndef _GETOPT_H_
#define _GETOPT_H_

 
#define no_argument        0
#define required_argument  1
#define optional_argument  2

#if 0
struct option {
	 
	const char *name;
	 
	int has_arg;
	 
	int *flag;
	 
	int val;
};

int	 getopt_long(int, char * const *, const char *,
	    const struct option *, int *);
int	 getopt_long_only(int, char * const *, const char *,
	    const struct option *, int *);
#endif

#ifndef _GETOPT_DEFINED_
#define _GETOPT_DEFINED_
int	 getopt(int, char * const *, const char *);
int	 getsubopt(char **, char * const *, char **);

extern   char *optarg;                   
extern   int opterr;
extern   int optind;
extern   int optopt;
extern   int optreset;
extern   char *suboptarg;                
#endif
 
#endif  
