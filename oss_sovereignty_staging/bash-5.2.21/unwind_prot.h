 

 

#if !defined (_UNWIND_PROT_H)
#define _UNWIND_PROT_H

extern void uwp_init PARAMS((void));

 
extern void begin_unwind_frame PARAMS((char *));
extern void discard_unwind_frame PARAMS((char *));
extern void run_unwind_frame PARAMS((char *));
extern void add_unwind_protect ();  
extern void remove_unwind_protect PARAMS((void));
extern void run_unwind_protects PARAMS((void));
extern void clear_unwind_protect_list PARAMS((int));
extern int have_unwind_protects PARAMS((void));
extern int unwind_protect_tag_on_stack PARAMS((const char *));
extern void uwp_init PARAMS((void));

 
#define end_unwind_frame()

 
#define unwind_protect_var(X) unwind_protect_mem ((char *)&(X), sizeof (X))
extern void unwind_protect_mem PARAMS((char *, int));

 
#define unwind_protect_int	unwind_protect_var
#define unwind_protect_short	unwind_protect_var
#define unwind_protect_string	unwind_protect_var
#define unwind_protect_pointer	unwind_protect_var
#define unwind_protect_jmp_buf	unwind_protect_var

#endif  
