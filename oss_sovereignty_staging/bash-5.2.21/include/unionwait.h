 

 

#ifndef _UNIONWAIT_H
#define _UNIONWAIT_H

#if !defined (WORDS_BIGENDIAN)
union wait
  {
    int	w_status;		 

     
    struct
      {
	unsigned short
	  w_Termsig  : 7,	 
	  w_Coredump : 1,	 
	  w_Retcode  : 8,	 
	  w_Fill1    : 16;	 
      } w_T;

     
    struct
      {
	unsigned short
	  w_Stopval : 8,	 
	  w_Stopsig : 8,	 
	  w_Fill2   : 16;	 
      } w_S;
  };

#else   

 

union wait
  {
    int	w_status;		 

     
    struct
      {
	unsigned short w_Fill1    : 16;	 
	unsigned       w_Retcode  : 8;	 
	unsigned       w_Coredump : 1;	 
	unsigned       w_Termsig  : 7;	 
      } w_T;

     
    struct
      {
	unsigned short w_Fill2   : 16;	 
	unsigned       w_Stopsig : 8;	 
	unsigned       w_Stopval : 8;	 
      } w_S;
  };

#endif  

#define w_termsig  w_T.w_Termsig
#define w_coredump w_T.w_Coredump
#define w_retcode  w_T.w_Retcode
#define w_stopval  w_S.w_Stopval
#define w_stopsig  w_S.w_Stopsig

#define WSTOPPED       0177
#define WIFSTOPPED(x)  ((x).w_stopval == WSTOPPED)
#define WIFEXITED(x)   ((x).w_stopval != WSTOPPED && (x).w_termsig == 0)
#define WIFSIGNALED(x) ((x).w_stopval != WSTOPPED && (x).w_termsig != 0)

#define WTERMSIG(x)    ((x).w_termsig)
#define WSTOPSIG(x)    ((x).w_stopsig)
#define WEXITSTATUS(x) ((x).w_retcode)
#define WIFCORED(x)    ((x).w_coredump)

#endif  
