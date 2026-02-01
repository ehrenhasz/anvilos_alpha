 
 
#ifndef __ODM_TYPES_H__
#define __ODM_TYPES_H__

#include <drv_types.h>

 
#define	ODM_ENDIAN_BIG	0
#define	ODM_ENDIAN_LITTLE	1

#define GET_ODM(__padapter)	((PDM_ODM_T)(&((GET_HAL_DATA(__padapter))->odmpriv)))

enum hal_status {
	HAL_STATUS_SUCCESS,
	HAL_STATUS_FAILURE,
	 
};


	#if defined(__LITTLE_ENDIAN)
		#define	ODM_ENDIAN_TYPE			ODM_ENDIAN_LITTLE
	#else
		#define	ODM_ENDIAN_TYPE			ODM_ENDIAN_BIG
	#endif

	#define	STA_INFO_T			struct sta_info
	#define	PSTA_INFO_T		struct sta_info *

	#define SET_TX_DESC_ANTSEL_A_88E(__pTxDesc, __Value) SET_BITS_TO_LE_4BYTE(__pTxDesc+8, 24, 1, __Value)
	#define SET_TX_DESC_ANTSEL_B_88E(__pTxDesc, __Value) SET_BITS_TO_LE_4BYTE(__pTxDesc+8, 25, 1, __Value)
	#define SET_TX_DESC_ANTSEL_C_88E(__pTxDesc, __Value) SET_BITS_TO_LE_4BYTE(__pTxDesc+28, 29, 1, __Value)

	 
	#define	USE_WORKITEM 0
	#define   FPGA_TWO_MAC_VERIFICATION	0

#define READ_NEXT_PAIR(v1, v2, i) do { if (i+2 >= ArrayLen) break; i += 2; v1 = Array[i]; v2 = Array[i+1]; } while (0)
#define COND_ELSE  2
#define COND_ENDIF 3

#endif  
