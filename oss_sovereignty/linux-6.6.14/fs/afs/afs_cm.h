 
 

#ifndef AFS_CM_H
#define AFS_CM_H

#define AFS_CM_PORT		7001	 
#define CM_SERVICE		1	 

enum AFS_CM_Operations {
	CBCallBack		= 204,	 
	CBInitCallBackState	= 205,	 
	CBProbe			= 206,	 
	CBGetLock		= 207,	 
	CBGetCE			= 208,	 
	CBGetXStatsVersion	= 209,	 
	CBGetXStats		= 210,	 
	CBInitCallBackState3	= 213,	 
	CBProbeUuid		= 214,	 
	CBTellMeAboutYourself	= 65538,  
};

#define AFS_CAP_ERROR_TRANSLATION	0x1

#endif  
