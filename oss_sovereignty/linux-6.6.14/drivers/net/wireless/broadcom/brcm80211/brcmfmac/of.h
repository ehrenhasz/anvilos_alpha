
 
#ifdef CONFIG_OF
void brcmf_of_probe(struct device *dev, enum brcmf_bus_type bus_type,
		    struct brcmf_mp_device *settings);
#else
static void brcmf_of_probe(struct device *dev, enum brcmf_bus_type bus_type,
			   struct brcmf_mp_device *settings)
{
}
#endif  
