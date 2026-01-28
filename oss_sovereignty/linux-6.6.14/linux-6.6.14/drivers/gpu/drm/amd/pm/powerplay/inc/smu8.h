#ifndef SMU8_H
#define SMU8_H
#pragma pack(push, 1)
#define ENABLE_DEBUG_FEATURES
struct SMU8_Firmware_Header {
	uint32_t Version;
	uint32_t ImageSize;
	uint32_t CodeSize;
	uint32_t HeaderSize;
	uint32_t EntryPoint;
	uint32_t Rtos;
	uint32_t UcodeLoadStatus;
	uint32_t DpmTable;
	uint32_t FanTable;
	uint32_t PmFuseTable;
	uint32_t Globals;
	uint32_t Reserved[20];
	uint32_t Signature;
};
struct SMU8_MultimediaPowerLogData {
	uint32_t avgTotalPower;
	uint32_t avgGpuPower;
	uint32_t avgUvdPower;
	uint32_t avgVcePower;
	uint32_t avgSclk;
	uint32_t avgDclk;
	uint32_t avgVclk;
	uint32_t avgEclk;
	uint32_t startTimeHi;
	uint32_t startTimeLo;
	uint32_t endTimeHi;
	uint32_t endTimeLo;
};
#define SMU8_FIRMWARE_HEADER_LOCATION 0x1FF80
#define SMU8_UNBCSR_START_ADDR 0xC0100000
#define SMN_MP1_SRAM_START_ADDR 0x10000000
#pragma pack(pop)
#endif
