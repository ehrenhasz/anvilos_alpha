#ifndef __SOUND_SOC_SOF_OF_H
#define __SOUND_SOC_SOF_OF_H
struct snd_sof_of_mach {
	const char *compatible;
	const char *drv_name;
	const char *fw_filename;
	const char *sof_tplg_filename;
};
extern const struct dev_pm_ops sof_of_pm;
int sof_of_probe(struct platform_device *pdev);
int sof_of_remove(struct platform_device *pdev);
void sof_of_shutdown(struct platform_device *pdev);
#endif
