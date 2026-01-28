#ifndef PP_THERMAL_H
#define PP_THERMAL_H
#include "power_state.h"
static const struct PP_TemperatureRange __maybe_unused SMU7ThermalWithDelayPolicy[] = {
	{-273150,  99000, 99000, -273150, 99000, 99000, -273150, 99000, 99000},
	{ 120000, 120000, 120000, 120000, 120000, 120000, 120000, 120000, 120000},
};
static const struct PP_TemperatureRange __maybe_unused SMU7ThermalPolicy[] = {
	{-273150,  99000, 99000, -273150, 99000, 99000, -273150, 99000, 99000},
	{ 120000, 120000, 120000, 120000, 120000, 120000, 120000, 120000, 120000},
};
#define CTF_OFFSET_EDGE			5
#define CTF_OFFSET_HOTSPOT		5
#define CTF_OFFSET_HBM			5
#endif
