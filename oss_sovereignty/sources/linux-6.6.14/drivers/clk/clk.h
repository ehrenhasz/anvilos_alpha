


struct clk_hw;
struct device;
struct of_phandle_args;

#if defined(CONFIG_OF) && defined(CONFIG_COMMON_CLK)
struct clk_hw *of_clk_get_hw(struct device_node *np,
				    int index, const char *con_id);
#else 
static inline struct clk_hw *of_clk_get_hw(struct device_node *np,
				    int index, const char *con_id)
{
	return ERR_PTR(-ENOENT);
}
#endif

struct clk_hw *clk_find_hw(const char *dev_id, const char *con_id);

#ifdef CONFIG_COMMON_CLK
struct clk *clk_hw_create_clk(struct device *dev, struct clk_hw *hw,
			      const char *dev_id, const char *con_id);
void __clk_put(struct clk *clk);
#else

static inline struct clk *
clk_hw_create_clk(struct device *dev, struct clk_hw *hw, const char *dev_id,
		  const char *con_id)
{
	return (struct clk *)hw;
}
static inline void __clk_put(struct clk *clk) { }

#endif
