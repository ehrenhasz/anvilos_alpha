

#ifndef __FSL_DPIO_H
#define __FSL_DPIO_H

struct fsl_mc_io;

int dpio_open(struct fsl_mc_io	*mc_io,
	      u32		cmd_flags,
	      int		dpio_id,
	      u16		*token);

int dpio_close(struct fsl_mc_io	*mc_io,
	       u32		cmd_flags,
	       u16		token);


enum dpio_channel_mode {
	DPIO_NO_CHANNEL = 0,
	DPIO_LOCAL_CHANNEL = 1,
};


struct dpio_cfg {
	enum dpio_channel_mode	channel_mode;
	u8		num_priorities;
};

int dpio_enable(struct fsl_mc_io	*mc_io,
		u32		cmd_flags,
		u16		token);

int dpio_disable(struct fsl_mc_io	*mc_io,
		 u32		cmd_flags,
		 u16		token);


struct dpio_attr {
	int			id;
	u64		qbman_portal_ce_offset;
	u64		qbman_portal_ci_offset;
	u16		qbman_portal_id;
	enum dpio_channel_mode	channel_mode;
	u8			num_priorities;
	u32		qbman_version;
	u32		clk;
};

int dpio_get_attributes(struct fsl_mc_io	*mc_io,
			u32		cmd_flags,
			u16		token,
			struct dpio_attr	*attr);

int dpio_set_stashing_destination(struct fsl_mc_io *mc_io,
				  u32 cmd_flags,
				  u16 token,
				  u8 dest);

int dpio_get_api_version(struct fsl_mc_io *mc_io,
			 u32 cmd_flags,
			 u16 *major_ver,
			 u16 *minor_ver);

int dpio_reset(struct fsl_mc_io	*mc_io,
	       u32 cmd_flags,
	       u16 token);

#endif 
