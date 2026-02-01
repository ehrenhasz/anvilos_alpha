 

#ifndef _BSD_WAITPID_H
#define _BSD_WAITPID_H

#ifndef HAVE_WAITPID
 
#undef WIFEXITED
#undef WIFSTOPPED
#undef WIFSIGNALED

 
#define _W_INT(w)	(*(int*)&(w))	 
#define WIFEXITED(w)	(!((_W_INT(w)) & 0377))
#define WIFSTOPPED(w)	((_W_INT(w)) & 0100)
#define WIFSIGNALED(w)	(!WIFEXITED(w) && !WIFSTOPPED(w))
#define WEXITSTATUS(w)	(int)(WIFEXITED(w) ? ((_W_INT(w) >> 8) & 0377) : -1)
#define WTERMSIG(w)	(int)(WIFSIGNALED(w) ? (_W_INT(w) & 0177) : -1)
#define WCOREFLAG	0x80
#define WCOREDUMP(w)	((_W_INT(w)) & WCOREFLAG)

 
pid_t waitpid(int, int *, int);

#endif  
#endif  
