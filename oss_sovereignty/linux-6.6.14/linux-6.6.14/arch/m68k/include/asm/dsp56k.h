struct dsp56k_upload {
	int len;
	char __user *bin;
};
struct dsp56k_host_flags {
	int dir;      
	int out;      
	int status;   
};
#define DSP56K_UPLOAD	        1     
#define DSP56K_SET_TX_WSIZE	2     
#define DSP56K_SET_RX_WSIZE	3     
#define DSP56K_HOST_FLAGS	4     
#define DSP56K_HOST_CMD         5     
