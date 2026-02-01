 

 
#ifndef _IPA_VERSION_H_
#define _IPA_VERSION_H_

 
enum ipa_version {
	IPA_VERSION_3_0,
	IPA_VERSION_3_1,
	IPA_VERSION_3_5,
	IPA_VERSION_3_5_1,
	IPA_VERSION_4_0,
	IPA_VERSION_4_1,
	IPA_VERSION_4_2,
	IPA_VERSION_4_5,
	IPA_VERSION_4_7,
	IPA_VERSION_4_9,
	IPA_VERSION_4_11,
	IPA_VERSION_5_0,
	IPA_VERSION_5_1,
	IPA_VERSION_5_5,
	IPA_VERSION_COUNT,			 
};

static inline bool ipa_version_supported(enum ipa_version version)
{
	switch (version) {
	case IPA_VERSION_3_1:
	case IPA_VERSION_3_5_1:
	case IPA_VERSION_4_2:
	case IPA_VERSION_4_5:
	case IPA_VERSION_4_7:
	case IPA_VERSION_4_9:
	case IPA_VERSION_4_11:
	case IPA_VERSION_5_0:
		return true;
	default:
		return false;
	}
}

 
enum gsi_ee_id {
	GSI_EE_AP		= 0x0,
	GSI_EE_MODEM		= 0x1,
	GSI_EE_UC		= 0x2,
	GSI_EE_TZ		= 0x3,
};

#endif  
