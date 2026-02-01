
 

 

#include "../ni_device_routes.h"
#include "all.h"

struct ni_device_routes ni_pci_6534_device_routes = {
	.device = "pci-6534",
	.routes = (struct ni_route_set[]){
		{
			.dest = NI_PFI(0),
			.src = (int[]){
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = NI_PFI(1),
			.src = (int[]){
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = NI_PFI(2),
			.src = (int[]){
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = NI_PFI(3),
			.src = (int[]){
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = NI_PFI(4),
			.src = (int[]){
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = NI_PFI(5),
			.src = (int[]){
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = NI_PFI(6),
			.src = (int[]){
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = NI_PFI(7),
			.src = (int[]){
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(0),
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(1),
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				TRIGGER_LINE(0),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(2),
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(3),
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(4),
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(5),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(5),
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(6),
				0,  
			}
		},
		{
			.dest = TRIGGER_LINE(6),
			.src = (int[]){
				NI_PFI(0),
				NI_PFI(1),
				NI_PFI(2),
				NI_PFI(3),
				NI_PFI(4),
				NI_PFI(5),
				NI_PFI(6),
				TRIGGER_LINE(0),
				TRIGGER_LINE(1),
				TRIGGER_LINE(2),
				TRIGGER_LINE(3),
				TRIGGER_LINE(4),
				TRIGGER_LINE(5),
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
