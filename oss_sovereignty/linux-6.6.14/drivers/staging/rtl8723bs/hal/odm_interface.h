 
 


#ifndef	__ODM_INTERFACE_H__
#define __ODM_INTERFACE_H__



 

#define _reg_all(_name)			ODM_##_name
#define _reg_ic(_name, _ic)		ODM_##_name##_ic
#define _bit_all(_name)			BIT_##_name
#define _bit_ic(_name, _ic)		BIT_##_name##_ic

 

#define _reg_11N(_name)			ODM_REG_##_name##_11N
#define _bit_11N(_name)			ODM_BIT_##_name##_11N

#define _cat(_name, _ic_type, _func) _func##_11N(_name)

 
 
 
#define ODM_REG(_name, _pDM_Odm)	_cat(_name, _pDM_Odm->SupportICType, _reg)
#define ODM_BIT(_name, _pDM_Odm)	_cat(_name, _pDM_Odm->SupportICType, _bit)

#endif	 
