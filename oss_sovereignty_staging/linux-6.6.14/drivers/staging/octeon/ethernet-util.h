 
 

 
static inline void *cvm_oct_get_buffer_ptr(union cvmx_buf_ptr packet_ptr)
{
	return cvmx_phys_to_ptr(((packet_ptr.s.addr >> 7) - packet_ptr.s.back)
				<< 7);
}

 
static inline int INTERFACE(int ipd_port)
{
	int interface;

	if (ipd_port == CVMX_PIP_NUM_INPUT_PORTS)
		return 10;
	interface = cvmx_helper_get_interface_num(ipd_port);
	if (interface >= 0)
		return interface;
	panic("Illegal ipd_port %d passed to %s\n", ipd_port, __func__);
}

 
static inline int INDEX(int ipd_port)
{
	return cvmx_helper_get_interface_index_num(ipd_port);
}
