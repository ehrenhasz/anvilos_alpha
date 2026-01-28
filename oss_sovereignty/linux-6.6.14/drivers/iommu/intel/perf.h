enum latency_type {
	DMAR_LATENCY_INV_IOTLB = 0,
	DMAR_LATENCY_INV_DEVTLB,
	DMAR_LATENCY_INV_IEC,
	DMAR_LATENCY_PRQ,
	DMAR_LATENCY_NUM
};
enum latency_count {
	COUNTS_10e2 = 0,	 
	COUNTS_10e3,		 
	COUNTS_10e4,		 
	COUNTS_10e5,		 
	COUNTS_10e6,		 
	COUNTS_10e7,		 
	COUNTS_10e8_plus,	 
	COUNTS_MIN,
	COUNTS_MAX,
	COUNTS_SUM,
	COUNTS_NUM
};
struct latency_statistic {
	bool enabled;
	u64 counter[COUNTS_NUM];
	u64 samples;
};
#ifdef CONFIG_DMAR_PERF
int dmar_latency_enable(struct intel_iommu *iommu, enum latency_type type);
void dmar_latency_disable(struct intel_iommu *iommu, enum latency_type type);
bool dmar_latency_enabled(struct intel_iommu *iommu, enum latency_type type);
void dmar_latency_update(struct intel_iommu *iommu, enum latency_type type,
			 u64 latency);
int dmar_latency_snapshot(struct intel_iommu *iommu, char *str, size_t size);
#else
static inline int
dmar_latency_enable(struct intel_iommu *iommu, enum latency_type type)
{
	return -EINVAL;
}
static inline void
dmar_latency_disable(struct intel_iommu *iommu, enum latency_type type)
{
}
static inline bool
dmar_latency_enabled(struct intel_iommu *iommu, enum latency_type type)
{
	return false;
}
static inline void
dmar_latency_update(struct intel_iommu *iommu, enum latency_type type, u64 latency)
{
}
static inline int
dmar_latency_snapshot(struct intel_iommu *iommu, char *str, size_t size)
{
	return 0;
}
#endif  
