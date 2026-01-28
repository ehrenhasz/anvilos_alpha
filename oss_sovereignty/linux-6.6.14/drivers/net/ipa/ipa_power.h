#ifndef _IPA_POWER_H_
#define _IPA_POWER_H_
struct device;
struct ipa;
struct ipa_power_data;
enum ipa_irq_id;
extern const struct dev_pm_ops ipa_pm_ops;
u32 ipa_core_clock_rate(struct ipa *ipa);
void ipa_power_modem_queue_stop(struct ipa *ipa);
void ipa_power_modem_queue_wake(struct ipa *ipa);
void ipa_power_modem_queue_active(struct ipa *ipa);
void ipa_power_retention(struct ipa *ipa, bool enable);
void ipa_power_suspend_handler(struct ipa *ipa, enum ipa_irq_id irq_id);
int ipa_power_setup(struct ipa *ipa);
void ipa_power_teardown(struct ipa *ipa);
struct ipa_power *ipa_power_init(struct device *dev,
				 const struct ipa_power_data *data);
void ipa_power_exit(struct ipa_power *power);
#endif  
