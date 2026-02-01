
 
#include <linux/coresight.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/property.h>
#include <linux/slab.h>

#include <dt-bindings/arm/coresight-cti-dt.h>

#include "coresight-cti.h"
#include "coresight-priv.h"

 
#define NR_V8PE_IN_SIGS		2
#define NR_V8PE_OUT_SIGS	3
#define NR_V8ETM_INOUT_SIGS	4

 
#define CTI_DT_CONNS		"trig-conns"

 
#define CTI_DT_V8ARCH_COMPAT	"arm,coresight-cti-v8-arch"
#define CTI_DT_CSDEV_ASSOC	"arm,cs-dev-assoc"
#define CTI_DT_TRIGIN_SIGS	"arm,trig-in-sigs"
#define CTI_DT_TRIGOUT_SIGS	"arm,trig-out-sigs"
#define CTI_DT_TRIGIN_TYPES	"arm,trig-in-types"
#define CTI_DT_TRIGOUT_TYPES	"arm,trig-out-types"
#define CTI_DT_FILTER_OUT_SIGS	"arm,trig-filters"
#define CTI_DT_CONN_NAME	"arm,trig-conn-name"
#define CTI_DT_CTM_ID		"arm,cti-ctm-id"

#ifdef CONFIG_OF
 
static int of_cti_get_cpu_at_node(const struct device_node *node)
{
	int cpu;
	struct device_node *dn;

	if (node == NULL)
		return -1;

	dn = of_parse_phandle(node, "cpu", 0);
	 
	if (!dn)
		return -1;
	cpu = of_cpu_node_to_id(dn);
	of_node_put(dn);

	 
	return (cpu < 0) ? -1 : cpu;
}

#else
static int of_cti_get_cpu_at_node(const struct device_node *node)
{
	return -1;
}

#endif

 
static int cti_plat_get_cpu_at_node(struct fwnode_handle *fwnode)
{
	if (is_of_node(fwnode))
		return of_cti_get_cpu_at_node(to_of_node(fwnode));
	return -1;
}

const char *cti_plat_get_node_name(struct fwnode_handle *fwnode)
{
	if (is_of_node(fwnode))
		return of_node_full_name(to_of_node(fwnode));
	return "unknown";
}

 
static const char *
cti_plat_get_csdev_or_node_name(struct fwnode_handle *fwnode,
				struct coresight_device **csdev)
{
	const char *name = NULL;
	*csdev = coresight_find_csdev_by_fwnode(fwnode);
	if (*csdev)
		name = dev_name(&(*csdev)->dev);
	else
		name = cti_plat_get_node_name(fwnode);
	return name;
}

static bool cti_plat_node_name_eq(struct fwnode_handle *fwnode,
				  const char *name)
{
	if (is_of_node(fwnode))
		return of_node_name_eq(to_of_node(fwnode), name);
	return false;
}

static int cti_plat_create_v8_etm_connection(struct device *dev,
					     struct cti_drvdata *drvdata)
{
	int ret = -ENOMEM, i;
	struct fwnode_handle *root_fwnode, *cs_fwnode;
	const char *assoc_name = NULL;
	struct coresight_device *csdev;
	struct cti_trig_con *tc = NULL;

	root_fwnode = dev_fwnode(dev);
	if (IS_ERR_OR_NULL(root_fwnode))
		return -EINVAL;

	 
	cs_fwnode = fwnode_find_reference(root_fwnode, CTI_DT_CSDEV_ASSOC, 0);
	if (IS_ERR(cs_fwnode))
		return 0;

	 
	tc = cti_allocate_trig_con(dev, NR_V8ETM_INOUT_SIGS,
				   NR_V8ETM_INOUT_SIGS);
	if (!tc)
		goto create_v8_etm_out;

	 
	tc->con_in->used_mask = 0xF0;  
	tc->con_out->used_mask = 0xF0;  

	 
	for (i = 0; i < NR_V8ETM_INOUT_SIGS; i++) {
		tc->con_in->sig_types[i] = ETM_EXTOUT;
		tc->con_out->sig_types[i] = ETM_EXTIN;
	}

	 
	assoc_name = cti_plat_get_csdev_or_node_name(cs_fwnode, &csdev);
	ret = cti_add_connection_entry(dev, drvdata, tc, csdev, assoc_name);

create_v8_etm_out:
	fwnode_handle_put(cs_fwnode);
	return ret;
}

 
static int cti_plat_create_v8_connections(struct device *dev,
					  struct cti_drvdata *drvdata)
{
	struct cti_device *cti_dev = &drvdata->ctidev;
	struct cti_trig_con *tc = NULL;
	int cpuid = 0;
	char cpu_name_str[16];
	int ret = -ENOMEM;

	 
	cpuid = cti_plat_get_cpu_at_node(dev_fwnode(dev));
	if (cpuid < 0) {
		dev_warn(dev,
			 "ARM v8 architectural CTI connection: missing cpu\n");
		return -EINVAL;
	}
	cti_dev->cpu = cpuid;

	 
	tc = cti_allocate_trig_con(dev, NR_V8PE_IN_SIGS, NR_V8PE_OUT_SIGS);
	if (!tc)
		goto of_create_v8_out;

	 
	tc->con_in->used_mask = 0x3;  
	tc->con_in->sig_types[0] = PE_DBGTRIGGER;
	tc->con_in->sig_types[1] = PE_PMUIRQ;
	tc->con_out->used_mask = 0x7;  
	tc->con_out->sig_types[0] = PE_EDBGREQ;
	tc->con_out->sig_types[1] = PE_DBGRESTART;
	tc->con_out->sig_types[2] = PE_CTIIRQ;
	scnprintf(cpu_name_str, sizeof(cpu_name_str), "cpu%d", cpuid);

	ret = cti_add_connection_entry(dev, drvdata, tc, NULL, cpu_name_str);
	if (ret)
		goto of_create_v8_out;

	 
	ret = cti_plat_create_v8_etm_connection(dev, drvdata);
	if (ret)
		goto of_create_v8_out;

	 
	drvdata->config.trig_out_filter |= 0x1;

of_create_v8_out:
	return ret;
}

static int cti_plat_check_v8_arch_compatible(struct device *dev)
{
	struct fwnode_handle *fwnode = dev_fwnode(dev);

	if (is_of_node(fwnode))
		return of_device_is_compatible(to_of_node(fwnode),
					       CTI_DT_V8ARCH_COMPAT);
	return 0;
}

static int cti_plat_count_sig_elements(const struct fwnode_handle *fwnode,
				       const char *name)
{
	int nr_elem = fwnode_property_count_u32(fwnode, name);

	return (nr_elem < 0 ? 0 : nr_elem);
}

static int cti_plat_read_trig_group(struct cti_trig_grp *tgrp,
				    const struct fwnode_handle *fwnode,
				    const char *grp_name)
{
	int idx, err = 0;
	u32 *values;

	if (!tgrp->nr_sigs)
		return 0;

	values = kcalloc(tgrp->nr_sigs, sizeof(u32), GFP_KERNEL);
	if (!values)
		return -ENOMEM;

	err = fwnode_property_read_u32_array(fwnode, grp_name,
					     values, tgrp->nr_sigs);

	if (!err) {
		 
		for (idx = 0; idx < tgrp->nr_sigs; idx++)
			tgrp->used_mask |= BIT(values[idx]);
	}

	kfree(values);
	return err;
}

static int cti_plat_read_trig_types(struct cti_trig_grp *tgrp,
				    const struct fwnode_handle *fwnode,
				    const char *type_name)
{
	int items, err = 0, nr_sigs;
	u32 *values = NULL, i;

	 
	nr_sigs = tgrp->nr_sigs;
	if (!nr_sigs)
		return 0;

	 
	items = cti_plat_count_sig_elements(fwnode, type_name);
	if (items > nr_sigs)
		return -EINVAL;

	 
	if (items) {
		values = kcalloc(items, sizeof(u32), GFP_KERNEL);
		if (!values)
			return -ENOMEM;

		err = fwnode_property_read_u32_array(fwnode, type_name,
						     values, items);
		if (err)
			goto read_trig_types_out;
	}

	 
	for (i = 0; i < nr_sigs; i++) {
		if (i < items) {
			tgrp->sig_types[i] =
				values[i] < CTI_TRIG_MAX ? values[i] : GEN_IO;
		} else {
			tgrp->sig_types[i] = GEN_IO;
		}
	}

read_trig_types_out:
	kfree(values);
	return err;
}

static int cti_plat_process_filter_sigs(struct cti_drvdata *drvdata,
					const struct fwnode_handle *fwnode)
{
	struct cti_trig_grp *tg = NULL;
	int err = 0, nr_filter_sigs;

	nr_filter_sigs = cti_plat_count_sig_elements(fwnode,
						     CTI_DT_FILTER_OUT_SIGS);
	if (nr_filter_sigs == 0)
		return 0;

	if (nr_filter_sigs > drvdata->config.nr_trig_max)
		return -EINVAL;

	tg = kzalloc(sizeof(*tg), GFP_KERNEL);
	if (!tg)
		return -ENOMEM;

	err = cti_plat_read_trig_group(tg, fwnode, CTI_DT_FILTER_OUT_SIGS);
	if (!err)
		drvdata->config.trig_out_filter |= tg->used_mask;

	kfree(tg);
	return err;
}

static int cti_plat_create_connection(struct device *dev,
				      struct cti_drvdata *drvdata,
				      struct fwnode_handle *fwnode)
{
	struct cti_trig_con *tc = NULL;
	int cpuid = -1, err = 0;
	struct coresight_device *csdev = NULL;
	const char *assoc_name = "unknown";
	char cpu_name_str[16];
	int nr_sigs_in, nr_sigs_out;

	 
	nr_sigs_in = cti_plat_count_sig_elements(fwnode, CTI_DT_TRIGIN_SIGS);
	nr_sigs_out = cti_plat_count_sig_elements(fwnode, CTI_DT_TRIGOUT_SIGS);

	if ((nr_sigs_in > drvdata->config.nr_trig_max) ||
	    (nr_sigs_out > drvdata->config.nr_trig_max))
		return -EINVAL;

	tc = cti_allocate_trig_con(dev, nr_sigs_in, nr_sigs_out);
	if (!tc)
		return -ENOMEM;

	 
	err = cti_plat_read_trig_group(tc->con_in, fwnode,
				       CTI_DT_TRIGIN_SIGS);
	if (err)
		goto create_con_err;

	err = cti_plat_read_trig_types(tc->con_in, fwnode,
				       CTI_DT_TRIGIN_TYPES);
	if (err)
		goto create_con_err;

	err = cti_plat_read_trig_group(tc->con_out, fwnode,
				       CTI_DT_TRIGOUT_SIGS);
	if (err)
		goto create_con_err;

	err = cti_plat_read_trig_types(tc->con_out, fwnode,
				       CTI_DT_TRIGOUT_TYPES);
	if (err)
		goto create_con_err;

	err = cti_plat_process_filter_sigs(drvdata, fwnode);
	if (err)
		goto create_con_err;

	 
	fwnode_property_read_string(fwnode, CTI_DT_CONN_NAME, &assoc_name);

	 
	cpuid = cti_plat_get_cpu_at_node(fwnode);
	if (cpuid >= 0) {
		drvdata->ctidev.cpu = cpuid;
		scnprintf(cpu_name_str, sizeof(cpu_name_str), "cpu%d", cpuid);
		assoc_name = cpu_name_str;
	} else {
		 
		struct fwnode_handle *cs_fwnode = fwnode_find_reference(fwnode,
									CTI_DT_CSDEV_ASSOC,
									0);
		if (!IS_ERR(cs_fwnode)) {
			assoc_name = cti_plat_get_csdev_or_node_name(cs_fwnode,
								     &csdev);
			fwnode_handle_put(cs_fwnode);
		}
	}
	 
	err = cti_add_connection_entry(dev, drvdata, tc, csdev, assoc_name);

create_con_err:
	return err;
}

static int cti_plat_create_impdef_connections(struct device *dev,
					      struct cti_drvdata *drvdata)
{
	int rc = 0;
	struct fwnode_handle *fwnode = dev_fwnode(dev);
	struct fwnode_handle *child = NULL;

	if (IS_ERR_OR_NULL(fwnode))
		return -EINVAL;

	fwnode_for_each_child_node(fwnode, child) {
		if (cti_plat_node_name_eq(child, CTI_DT_CONNS))
			rc = cti_plat_create_connection(dev, drvdata,
							child);
		if (rc != 0)
			break;
	}
	fwnode_handle_put(child);

	return rc;
}

 
static int cti_plat_get_hw_data(struct device *dev, struct cti_drvdata *drvdata)
{
	int rc = 0;
	struct cti_device *cti_dev = &drvdata->ctidev;

	 
	device_property_read_u32(dev, CTI_DT_CTM_ID, &cti_dev->ctm_id);

	 
	if (cti_plat_check_v8_arch_compatible(dev))
		rc = cti_plat_create_v8_connections(dev, drvdata);
	else
		rc = cti_plat_create_impdef_connections(dev, drvdata);
	if (rc)
		return rc;

	 
	if (cti_dev->nr_trig_con == 0)
		rc = cti_add_default_connection(dev, drvdata);
	return rc;
}

struct coresight_platform_data *
coresight_cti_get_platform_data(struct device *dev)
{
	int ret = -ENOENT;
	struct coresight_platform_data *pdata = NULL;
	struct fwnode_handle *fwnode = dev_fwnode(dev);
	struct cti_drvdata *drvdata = dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(fwnode))
		goto error;

	 
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto error;
	}

	 
	ret = cti_plat_get_hw_data(dev, drvdata);

	if (!ret)
		return pdata;
error:
	return ERR_PTR(ret);
}
