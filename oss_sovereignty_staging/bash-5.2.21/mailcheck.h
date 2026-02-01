 

 

#if !defined (_MAILCHECK_H_)
#define _MAILCHECK_H_

 
extern int time_to_check_mail PARAMS((void));
extern void reset_mail_timer PARAMS((void));
extern void reset_mail_files PARAMS((void));
extern void free_mail_files PARAMS((void));
extern char *make_default_mailpath PARAMS((void));
extern void remember_mail_dates PARAMS((void));
extern void init_mail_dates PARAMS((void));
extern void check_mail PARAMS((void));

#endif  
