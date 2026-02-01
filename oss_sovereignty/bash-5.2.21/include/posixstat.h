 

 

 
#if !defined (_POSIXSTAT_H_)
#define _POSIXSTAT_H_

#include <sys/stat.h>

#if defined (STAT_MACROS_BROKEN)
#  undef S_ISBLK
#  undef S_ISCHR
#  undef S_ISDIR
#  undef S_ISFIFO
#  undef S_ISREG
#  undef S_ISLNK
#endif  

 
#if !defined (S_IFDIR) && !defined (S_ISDIR)
#  define S_IFDIR 0040000
#endif  
#if !defined (S_IFMT)
#  define S_IFMT  0170000
#endif  

 

 

#if defined (_S_IFMT) && !defined (S_IFMT)
#define S_IFMT _S_IFMT
#endif
#if defined (_S_IFIFO) && !defined (S_IFIFO)
#define S_IFIFO _S_IFIFO
#endif
#if defined (_S_IFCHR) && !defined (S_IFCHR)
#define S_IFCHR _S_IFCHR
#endif
#if defined (_S_IFDIR) && !defined (S_IFDIR)
#define S_IFDIR _S_IFDIR
#endif
#if defined (_S_IFBLK) && !defined (S_IFBLK)
#define S_IFBLK _S_IFBLK
#endif
#if defined (_S_IFREG) && !defined (S_IFREG)
#define S_IFREG _S_IFREG
#endif
#if defined (_S_IFLNK) && !defined (S_IFLNK)
#define S_IFLNK _S_IFLNK
#endif
#if defined (_S_IFSOCK) && !defined (S_IFSOCK)
#define S_IFSOCK _S_IFSOCK
#endif

 

#if defined (S_IFBLK) && !defined (S_ISBLK)
#define	S_ISBLK(m)	(((m)&S_IFMT) == S_IFBLK)	 
#endif

#if defined (S_IFCHR) && !defined (S_ISCHR)
#define	S_ISCHR(m)	(((m)&S_IFMT) == S_IFCHR)	 
#endif

#if defined (S_IFDIR) && !defined (S_ISDIR)
#define	S_ISDIR(m)	(((m)&S_IFMT) == S_IFDIR)	 
#endif

#if defined (S_IFREG) && !defined (S_ISREG)
#define	S_ISREG(m)	(((m)&S_IFMT) == S_IFREG)	 
#endif

#if defined (S_IFIFO) && !defined (S_ISFIFO)
#define	S_ISFIFO(m)	(((m)&S_IFMT) == S_IFIFO)	 
#endif

#if defined (S_IFLNK) && !defined (S_ISLNK)
#define	S_ISLNK(m)	(((m)&S_IFMT) == S_IFLNK)	 
#endif

#if defined (S_IFSOCK) && !defined (S_ISSOCK)
#define	S_ISSOCK(m)	(((m)&S_IFMT) == S_IFSOCK)	 
#endif

 

#if !defined (S_IRWXU)
#  if !defined (S_IREAD)
#    define S_IREAD	00400
#    define S_IWRITE	00200
#    define S_IEXEC	00100
#  endif  

#  if !defined (S_IRUSR)
#    define S_IRUSR	S_IREAD			 
#    define S_IWUSR	S_IWRITE		 
#    define S_IXUSR	S_IEXEC			 

#    define S_IRGRP	(S_IREAD  >> 3)		 
#    define S_IWGRP	(S_IWRITE >> 3)		 
#    define S_IXGRP	(S_IEXEC  >> 3)		 

#    define S_IROTH	(S_IREAD  >> 6)		 
#    define S_IWOTH	(S_IWRITE >> 6)		 
#    define S_IXOTH	(S_IEXEC  >> 6)		 
#  endif  

#  define S_IRWXU	(S_IRUSR | S_IWUSR | S_IXUSR)
#  define S_IRWXG	(S_IRGRP | S_IWGRP | S_IXGRP)
#  define S_IRWXO	(S_IROTH | S_IWOTH | S_IXOTH)
#else  
   
#  if !defined (S_IRGRP)
#    define S_IRGRP	(S_IREAD  >> 3)		 
#    define S_IWGRP	(S_IWRITE >> 3)		 
#    define S_IXGRP	(S_IEXEC  >> 3)		 
#  endif  

#  if !defined (S_IROTH)
#    define S_IROTH	(S_IREAD  >> 6)		 
#    define S_IWOTH	(S_IWRITE >> 6)		 
#    define S_IXOTH	(S_IEXEC  >> 6)		 
#  endif  
#  if !defined (S_IRWXG)
#    define S_IRWXG	(S_IRGRP | S_IWGRP | S_IXGRP)
#  endif
#  if !defined (S_IRWXO)
#    define S_IRWXO	(S_IROTH | S_IWOTH | S_IXOTH)
#  endif
#endif  

 
#define S_IRUGO		(S_IRUSR | S_IRGRP | S_IROTH)
#define S_IWUGO		(S_IWUSR | S_IWGRP | S_IWOTH)
#define S_IXUGO		(S_IXUSR | S_IXGRP | S_IXOTH)

#endif  
