struct regmap;
enum icst_control_type {
	ICST_VERSATILE,  
	ICST_INTEGRATOR_AP_CM,  
	ICST_INTEGRATOR_AP_SYS,  
	ICST_INTEGRATOR_AP_PCI,  
	ICST_INTEGRATOR_CP_CM_CORE,  
	ICST_INTEGRATOR_CP_CM_MEM,  
	ICST_INTEGRATOR_IM_PD1,  
};
struct clk_icst_desc {
	const struct icst_params *params;
	u32 vco_offset;
	u32 lock_offset;
};
struct clk *icst_clk_register(struct device *dev,
			      const struct clk_icst_desc *desc,
			      const char *name,
			      const char *parent_name,
			      void __iomem *base);
struct clk *icst_clk_setup(struct device *dev,
			   const struct clk_icst_desc *desc,
			   const char *name,
			   const char *parent_name,
			   struct regmap *map,
			   enum icst_control_type ctype);
