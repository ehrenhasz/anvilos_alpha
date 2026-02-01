
 

 

#include "../ni_device_routes.h"
#include "all.h"

struct ni_device_routes ni_pci_6070e_device_routes = {
	.device = "pci-6070e",
	.routes = (struct ni_route_set[]){
		{
			.dest = NI_PFI(0),
			.src = (int[]){
				NI_AI_StartTrigger,
				0,  
			}
		},
		{
			.dest = NI_PFI(1),
			.src = (int[]){
				NI_AI_ReferenceTrigger,
				0,  
			}
		},
		{
			.dest = NI_PFI(2),
			.src = (int[]){
				NI_AI_ConvertClock,
				0,  
			}
		},
		{
			.dest = NI_PFI(3),
			.src = (int[]){
				NI_CtrSource(1),
				0,  
			}
		},
		{
			.dest = NI_PFI(4),
			.src = (int[]){
				NI_CtrGate(1),
				0,  
			}
		},
		{
			.dest = NI_PFI(5),
			.src = (int[]){
				NI_AO_SampleClock,
				0,  
			}
		},
		{
			.dest = NI_PFI(6),
			.src = (int[]){
				NI_AO_StartTrigger,
				0,  
			}
		},
		{
			.dest = NI_PFI(7),
			.src = (int[]){
				NI_AI_SampleClock,
				0,  
			}
		},
		{
			.dest = NI_PFI(8),
			.src = (int[]){
				NI_CtrSource(0),
				0,  
			}
		},
		{
			.dest = NI_PFI(9),
			.src = (int[]){
				NI_CtrGate(0),
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(0),
			.src = (int[]){
				NI_CtrSource(0),
				NI_CtrGate(0),
				NI_CtrInternalOutput(0),
				NI_CtrOut(0),
				NI_AI_SampleClock,
				NI_AI_StartTrigger,
				NI_AI_ReferenceTrigger,
				NI_AI_ConvertClock,
				NI_AO_SampleClock,
				NI_AO_StartTrigger,
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(1),
			.src = (int[]){
				NI_CtrSource(0),
				NI_CtrGate(0),
				NI_CtrInternalOutput(0),
				NI_CtrOut(0),
				NI_AI_SampleClock,
				NI_AI_StartTrigger,
				NI_AI_ReferenceTrigger,
				NI_AI_ConvertClock,
				NI_AO_SampleClock,
				NI_AO_StartTrigger,
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(2),
			.src = (int[]){
				NI_CtrSource(0),
				NI_CtrGate(0),
				NI_CtrInternalOutput(0),
				NI_CtrOut(0),
				NI_AI_SampleClock,
				NI_AI_StartTrigger,
				NI_AI_ReferenceTrigger,
				NI_AI_ConvertClock,
				NI_AO_SampleClock,
				NI_AO_StartTrigger,
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(3),
			.src = (int[]){
				NI_CtrSource(0),
				NI_CtrGate(0),
				NI_CtrInternalOutput(0),
				NI_CtrOut(0),
				NI_AI_SampleClock,
				NI_AI_StartTrigger,
				NI_AI_ReferenceTrigger,
				NI_AI_ConvertClock,
				NI_AO_SampleClock,
				NI_AO_StartTrigger,
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(4),
			.src = (int[]){
				NI_CtrSource(0),
				NI_CtrGate(0),
				NI_CtrInternalOutput(0),
				NI_CtrOut(0),
				NI_AI_SampleClock,
				NI_AI_StartTrigger,
				NI_AI_ReferenceTrigger,
				NI_AI_ConvertClock,
				NI_AO_SampleClock,
				NI_AO_StartTrigger,
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(5),
			.src = (int[]){
				NI_CtrSource(0),
				NI_CtrGate(0),
				NI_CtrInternalOutput(0),
				NI_CtrOut(0),
				NI_AI_SampleClock,
				NI_AI_StartTrigger,
				NI_AI_ReferenceTrigger,
				NI_AI_ConvertClock,
				NI_AO_SampleClock,
				NI_AO_StartTrigger,
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(6),
			.src = (int[]){
				NI_CtrSource(0),
				NI_CtrGate(0),
				NI_CtrInternalOutput(0),
				NI_CtrOut(0),
				NI_AI_SampleClock,
				NI_AI_StartTrigger,
				NI_AI_ReferenceTrigger,
				NI_AI_ConvertClock,
				NI_AO_SampleClock,
				NI_AO_StartTrigger,
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(7),
			.src = (int[]){
				NI_20MHzTimebase,
				0,  
			}
		},
		{
			.dest = NI_CtrSource(0),
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				TRIGGER_LINE(7),
				NI_MasterTimebase,
				NI_20MHzTimebase,
				NI_100kHzTimebase,
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_CtrSource(1),
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				TRIGGER_LINE(7),
				NI_MasterTimebase,
				NI_20MHzTimebase,
				NI_100kHzTimebase,
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_CtrGate(0),
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				NI_CtrInternalOutput(1),
				NI_AI_StartTrigger,
				NI_AI_ReferenceTrigger,
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_CtrGate(1),
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				NI_CtrInternalOutput(0),
				NI_AI_StartTrigger,
				NI_AI_ReferenceTrigger,
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_CtrOut(0),
			.src = (int[]){
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				NI_CtrInternalOutput(0),
				0,  
			}
		},
		{
			.dest = NI_CtrOut(1),
			.src = (int[]){
				NI_CtrInternalOutput(1),
				0,  
			}
		},
		{
			.dest = NI_AI_SampleClock,
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				NI_CtrInternalOutput(0),
				NI_AI_SampleClockTimebase,
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_AI_SampleClockTimebase,
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				TRIGGER_LINE(7),
				NI_MasterTimebase,
				NI_20MHzTimebase,
				NI_100kHzTimebase,
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_AI_StartTrigger,
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				NI_CtrInternalOutput(0),
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_AI_ReferenceTrigger,
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_AI_ConvertClock,
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				NI_CtrInternalOutput(0),
				NI_AI_ConvertClockTimebase,
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_AI_ConvertClockTimebase,
			.src = (int[]){
				TRIGGER_LINE(7),
				NI_AI_SampleClockTimebase,
				NI_MasterTimebase,
				NI_20MHzTimebase,
				0,  
			}
		},
		{
			.dest = NI_AI_PauseTrigger,
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_AI_HoldComplete,
			.src = (int[]){
				NI_AI_HoldCompleteEvent,
				0,  
			}
		},
		{
			.dest = NI_AO_SampleClock,
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				NI_CtrInternalOutput(1),
				NI_AO_SampleClockTimebase,
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_AO_SampleClockTimebase,
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				TRIGGER_LINE(7),
				NI_MasterTimebase,
				NI_20MHzTimebase,
				NI_100kHzTimebase,
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_AO_StartTrigger,
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				NI_AI_StartTrigger,
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_AO_PauseTrigger,
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				NI_PFI(7),
				NI_PFI(8),
				NI_PFI(9),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				NI_AnalogComparisonEvent,
				0,  
			}
		},
		{
			.dest = NI_MasterTimebase,
			.src = (int[]){
				TRIGGER_LINE(7),
				NI_20MHzTimebase,
				0,  
			}
		},
		{  
			.dest = 0,
		},
	},
};
