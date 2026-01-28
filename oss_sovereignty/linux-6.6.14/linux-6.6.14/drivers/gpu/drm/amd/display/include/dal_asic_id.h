#ifndef __DAL_ASIC_ID_H__
#define __DAL_ASIC_ID_H__
#define SI_TAHITI_P_A0    0x01
#define SI_TAHITI_P_B0    0x05
#define SI_TAHITI_P_B1    0x06
#define SI_PITCAIRN_PM_A0 0x14
#define SI_PITCAIRN_PM_A1 0x15
#define SI_CAPEVERDE_M_A0 0x28
#define SI_CAPEVERDE_M_A1 0x29
#define SI_OLAND_M_A0     0x3C
#define SI_HAINAN_V_A0    0x46
#define SI_UNKNOWN        0xFF
#define ASIC_REV_IS_TAHITI_P(rev) \
	((rev >= SI_TAHITI_P_A0) && (rev < SI_PITCAIRN_PM_A0))
#define ASIC_REV_IS_PITCAIRN_PM(rev) \
	((rev >= SI_PITCAIRN_PM_A0) && (rev < SI_CAPEVERDE_M_A0))
#define ASIC_REV_IS_CAPEVERDE_M(rev) \
	((rev >= SI_CAPEVERDE_M_A0) && (rev < SI_OLAND_M_A0))
#define ASIC_REV_IS_OLAND_M(rev) \
	((rev >= SI_OLAND_M_A0) && (rev < SI_HAINAN_V_A0))
#define ASIC_REV_IS_HAINAN_V(rev) \
	((rev >= SI_HAINAN_V_A0) && (rev < SI_UNKNOWN))
#define	CI_BONAIRE_M_A0 0x14
#define	CI_BONAIRE_M_A1	0x15
#define	CI_HAWAII_P_A0	0x28
#define CI_UNKNOWN	0xFF
#define ASIC_REV_IS_BONAIRE_M(rev) \
	((rev >= CI_BONAIRE_M_A0) && (rev < CI_HAWAII_P_A0))
#define ASIC_REV_IS_HAWAII_P(rev) \
	(rev >= CI_HAWAII_P_A0)
#define KV_SPECTRE_A0 0x01
#define KV_SPOOKY_A0 0x41
#define KB_KALINDI_A0 0x81
#define KB_KALINDI_A1 0x82
#define BV_KALINDI_A2 0x85
#define ML_GODAVARI_A0 0xA1
#define ML_GODAVARI_A1 0xA2
#define KV_UNKNOWN 0xFF
#define ASIC_REV_IS_KALINDI(rev) \
	((rev >= KB_KALINDI_A0) && (rev < KV_UNKNOWN))
#define ASIC_REV_IS_BHAVANI(rev) \
	((rev >= BV_KALINDI_A2) && (rev < ML_GODAVARI_A0))
#define ASIC_REV_IS_GODAVARI(rev) \
	((rev >= ML_GODAVARI_A0) && (rev < KV_UNKNOWN))
#define VI_TONGA_P_A0 20
#define VI_TONGA_P_A1 21
#define VI_FIJI_P_A0 60
#define VI_POLARIS10_P_A0 80
#define VI_POLARIS11_M_A0 90
#define VI_POLARIS12_V_A0 100
#define VI_VEGAM_A0 110
#define VI_UNKNOWN 0xFF
#define ASIC_REV_IS_TONGA_P(eChipRev) ((eChipRev >= VI_TONGA_P_A0) && \
		(eChipRev < 40))
#define ASIC_REV_IS_FIJI_P(eChipRev) ((eChipRev >= VI_FIJI_P_A0) && \
		(eChipRev < 80))
#define ASIC_REV_IS_POLARIS10_P(eChipRev) ((eChipRev >= VI_POLARIS10_P_A0) && \
		(eChipRev < VI_POLARIS11_M_A0))
#define ASIC_REV_IS_POLARIS11_M(eChipRev) ((eChipRev >= VI_POLARIS11_M_A0) &&  \
		(eChipRev < VI_POLARIS12_V_A0))
#define ASIC_REV_IS_POLARIS12_V(eChipRev) ((eChipRev >= VI_POLARIS12_V_A0) && \
		(eChipRev < VI_VEGAM_A0))
#define ASIC_REV_IS_VEGAM(eChipRev) (eChipRev >= VI_VEGAM_A0)
#define CZ_CARRIZO_A0 0x01
#define STONEY_A0 0x61
#define CZ_UNKNOWN 0xFF
#define ASIC_REV_IS_STONEY(rev) \
	((rev >= STONEY_A0) && (rev < CZ_UNKNOWN))
#define AI_UNKNOWN 0xFF
#define AI_GREENLAND_P_A0 1
#define AI_GREENLAND_P_A1 2
#define AI_UNKNOWN 0xFF
#define AI_VEGA12_P_A0 20
#define AI_VEGA20_P_A0 40
#define ASICREV_IS_GREENLAND_M(eChipRev)  (eChipRev < AI_VEGA12_P_A0)
#define ASICREV_IS_GREENLAND_P(eChipRev)  (eChipRev < AI_VEGA12_P_A0)
#define ASICREV_IS_VEGA12_P(eChipRev) ((eChipRev >= AI_VEGA12_P_A0) && (eChipRev < AI_VEGA20_P_A0))
#define ASICREV_IS_VEGA20_P(eChipRev) ((eChipRev >= AI_VEGA20_P_A0) && (eChipRev < AI_UNKNOWN))
#define INTERNAL_REV_RAVEN_A0             0x00     
#define RAVEN_A0 0x01
#define RAVEN_B0 0x21
#define PICASSO_A0 0x41
#define RAVEN2_A0 0x81
#define RAVEN1_F0 0xF0
#define RAVEN_UNKNOWN 0xFF
#define RENOIR_A0 0x91
#ifndef ASICREV_IS_RAVEN
#define ASICREV_IS_RAVEN(eChipRev) ((eChipRev >= RAVEN_A0) && eChipRev < RAVEN_UNKNOWN)
#endif
#define PRID_DALI_DE 0xDE
#define PRID_DALI_DF 0xDF
#define PRID_DALI_E3 0xE3
#define PRID_DALI_E4 0xE4
#define PRID_POLLOCK_94 0x94
#define PRID_POLLOCK_95 0x95
#define PRID_POLLOCK_E9 0xE9
#define PRID_POLLOCK_EA 0xEA
#define PRID_POLLOCK_EB 0xEB
#define ASICREV_IS_PICASSO(eChipRev) ((eChipRev >= PICASSO_A0) && (eChipRev < RAVEN2_A0))
#ifndef ASICREV_IS_RAVEN2
#define ASICREV_IS_RAVEN2(eChipRev) ((eChipRev >= RAVEN2_A0) && (eChipRev < RENOIR_A0))
#endif
#define ASICREV_IS_RV1_F0(eChipRev) ((eChipRev >= RAVEN1_F0) && (eChipRev < RAVEN_UNKNOWN))
#define FAMILY_RV 142  
#define FAMILY_NV 143  
enum {
	NV_NAVI10_P_A0      = 1,
	NV_NAVI12_P_A0      = 10,
	NV_NAVI14_M_A0      = 20,
	NV_SIENNA_CICHLID_P_A0      = 40,
	NV_DIMGREY_CAVEFISH_P_A0      = 60,
	NV_BEIGE_GOBY_P_A0  = 70,
	NV_UNKNOWN          = 0xFF
};
#define ASICREV_IS_NAVI10_P(eChipRev)        (eChipRev < NV_NAVI12_P_A0)
#define ASICREV_IS_NAVI12_P(eChipRev)        ((eChipRev >= NV_NAVI12_P_A0) && (eChipRev < NV_NAVI14_M_A0))
#define ASICREV_IS_NAVI14_M(eChipRev)        ((eChipRev >= NV_NAVI14_M_A0) && (eChipRev < NV_UNKNOWN))
#define ASICREV_IS_RENOIR(eChipRev) ((eChipRev >= RENOIR_A0) && (eChipRev < RAVEN1_F0))
#define ASICREV_IS_SIENNA_CICHLID_P(eChipRev)        ((eChipRev >= NV_SIENNA_CICHLID_P_A0) && (eChipRev < NV_DIMGREY_CAVEFISH_P_A0))
#define ASICREV_IS_DIMGREY_CAVEFISH_P(eChipRev)        ((eChipRev >= NV_DIMGREY_CAVEFISH_P_A0) && (eChipRev < NV_BEIGE_GOBY_P_A0))
#define ASICREV_IS_BEIGE_GOBY_P(eChipRev)        ((eChipRev >= NV_BEIGE_GOBY_P_A0) && (eChipRev < NV_UNKNOWN))
#define GREEN_SARDINE_A0 0xA1
#ifndef ASICREV_IS_GREEN_SARDINE
#define ASICREV_IS_GREEN_SARDINE(eChipRev) ((eChipRev >= GREEN_SARDINE_A0) && (eChipRev < 0xFF))
#endif
#define DEVICE_ID_NV_13FE 0x13FE   
#define DEVICE_ID_NV_143F 0x143F
#define FAMILY_VGH 144
#define DEVICE_ID_VGH_163F 0x163F
#define DEVICE_ID_VGH_1435 0x1435
#define VANGOGH_A0 0x01
#define VANGOGH_UNKNOWN 0xFF
#ifndef ASICREV_IS_VANGOGH
#define ASICREV_IS_VANGOGH(eChipRev) ((eChipRev >= VANGOGH_A0) && (eChipRev < VANGOGH_UNKNOWN))
#endif
#define FAMILY_YELLOW_CARP                     146
#define YELLOW_CARP_A0 0x01
#define YELLOW_CARP_B0 0x20
#define YELLOW_CARP_UNKNOWN 0xFF
#ifndef ASICREV_IS_YELLOW_CARP
#define ASICREV_IS_YELLOW_CARP(eChipRev) ((eChipRev >= YELLOW_CARP_A0) && (eChipRev < YELLOW_CARP_UNKNOWN))
#endif
#define AMDGPU_FAMILY_GC_10_3_6                     149
#define GC_10_3_6_A0            0x01
#define GC_10_3_6_UNKNOWN       0xFF
#define ASICREV_IS_GC_10_3_6(eChipRev) ((eChipRev >= GC_10_3_6_A0) && (eChipRev < GC_10_3_6_UNKNOWN))
#define AMDGPU_FAMILY_GC_10_3_7                151
#define GC_10_3_7_A0 0x01
#define GC_10_3_7_UNKNOWN 0xFF
#define ASICREV_IS_GC_10_3_7(eChipRev) ((eChipRev >= GC_10_3_7_A0) && (eChipRev < GC_10_3_7_UNKNOWN))
#define AMDGPU_FAMILY_GC_11_0_0 145
#define AMDGPU_FAMILY_GC_11_0_1 148
#define AMDGPU_FAMILY_GC_11_5_0 150
#define GC_11_0_0_A0 0x1
#define GC_11_0_2_A0 0x10
#define GC_11_0_3_A0 0x20
#define GC_11_UNKNOWN 0xFF
#define ASICREV_IS_GC_11_0_0(eChipRev) (eChipRev < GC_11_0_2_A0)
#define ASICREV_IS_GC_11_0_2(eChipRev) (eChipRev >= GC_11_0_2_A0 && eChipRev < GC_11_0_3_A0)
#define ASICREV_IS_GC_11_0_3(eChipRev) (eChipRev >= GC_11_0_3_A0 && eChipRev < GC_11_UNKNOWN)
#define DEVICE_ID_SI_TAHITI_P_6780 0x6780
#define DEVICE_ID_SI_PITCAIRN_PM_6800 0x6800
#define DEVICE_ID_SI_PITCAIRN_PM_6808 0x6808
#define DEVICE_ID_SI_CAPEVERDE_M_6820 0x6820
#define DEVICE_ID_SI_CAPEVERDE_M_6828 0x6828
#define DEVICE_ID_SI_OLAND_M_6600 0x6600
#define DEVICE_ID_SI_OLAND_M_6608 0x6608
#define DEVICE_ID_SI_HAINAN_V_6660 0x6660
#define DEVICE_ID_KALINDI_9834 0x9834
#define DEVICE_ID_TEMASH_9839 0x9839
#define DEVICE_ID_TEMASH_983D 0x983D
#define DEVICE_ID_RENOIR_1636 0x1636
#define FAMILY_SI 110  
#define FAMILY_CI 120  
#define FAMILY_KV 125  
#define FAMILY_VI 130  
#define FAMILY_CZ 135  
#define FAMILY_AI 141
#define	FAMILY_UNKNOWN 0xFF
#endif  
