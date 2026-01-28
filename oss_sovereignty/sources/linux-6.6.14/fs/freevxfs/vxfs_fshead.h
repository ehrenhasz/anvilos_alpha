

#ifndef _VXFS_FSHEAD_H_
#define _VXFS_FSHEAD_H_





struct vxfs_fsh {
	__fs32		fsh_version;		
	__fs32		fsh_fsindex;		
	__fs32		fsh_time;		
	__fs32		fsh_utime;		
	__fs32		fsh_extop;		
	__fs32		fsh_ninodes;		
	__fs32		fsh_nau;		
	__fs32		fsh_old_ilesize;	
	__fs32		fsh_dflags;		
	__fs32		fsh_quota;		
	__fs32		fsh_maxinode;		
	__fs32		fsh_iauino;		
	__fs32		fsh_ilistino[2];	
	__fs32		fsh_lctino;		

	
};

#endif 
