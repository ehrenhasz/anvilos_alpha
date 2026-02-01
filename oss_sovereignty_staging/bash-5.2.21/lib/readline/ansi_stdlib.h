 
 

 

#if !defined (_STDLIB_H_)
#define	_STDLIB_H_ 1

 
extern int atoi ();

extern double atof ();
extern double strtod ();

 
 
#ifndef PTR_T

#if defined (__STDC__)
#  define PTR_T	void *
#else
#  define PTR_T char *
#endif

#endif  

extern PTR_T malloc ();
extern PTR_T realloc ();
extern void free ();

 
extern void abort ();
extern void exit ();
extern char *getenv ();
extern void qsort ();

#endif  
