



CFLAGS_core.o = -I$(src)

obj-$(CONFIG_SURFACE_AGGREGATOR) += surface_aggregator.o

surface_aggregator-y := core.o
surface_aggregator-y += ssh_parser.o
surface_aggregator-y += ssh_packet_layer.o
surface_aggregator-y += ssh_request_layer.o
surface_aggregator-$(CONFIG_SURFACE_AGGREGATOR_BUS) += bus.o
surface_aggregator-y += controller.o
