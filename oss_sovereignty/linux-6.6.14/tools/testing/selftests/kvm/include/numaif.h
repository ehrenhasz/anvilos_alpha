 
 

#ifndef SELFTEST_KVM_NUMAIF_H
#define SELFTEST_KVM_NUMAIF_H

#define __NR_get_mempolicy 239
#define __NR_migrate_pages 256

 
long get_mempolicy(int *policy, const unsigned long *nmask,
		   unsigned long maxnode, void *addr, int flags)
{
	return syscall(__NR_get_mempolicy, policy, nmask,
		       maxnode, addr, flags);
}

long migrate_pages(int pid, unsigned long maxnode,
		   const unsigned long *frommask,
		   const unsigned long *tomask)
{
	return syscall(__NR_migrate_pages, pid, maxnode, frommask, tomask);
}

 
#define MPOL_DEFAULT	 0
#define MPOL_PREFERRED	 1
#define MPOL_BIND	 2
#define MPOL_INTERLEAVE	 3

#define MPOL_MAX MPOL_INTERLEAVE

 
#define MPOL_F_NODE	    (1<<0)   
				     
#define MPOL_F_ADDR	    (1<<1)   
#define MPOL_F_MEMS_ALLOWED (1<<2)   

 
#define MPOL_MF_STRICT	     (1<<0)  
#define MPOL_MF_MOVE	     (1<<1)  
#define MPOL_MF_MOVE_ALL     (1<<2)  

#endif  
