
 

#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/export.h>

#include "internal.h"

static int acpi_data_get_property_array(const struct acpi_device_data *data,
					const char *name,
					acpi_object_type type,
					const union acpi_object **obj);

 
static const guid_t prp_guids[] = {
	 
	GUID_INIT(0xdaffd814, 0x6eba, 0x4d8c,
		  0x8a, 0x91, 0xbc, 0x9b, 0xbf, 0x4a, 0xa3, 0x01),
	 
	GUID_INIT(0x6211e2c0, 0x58a3, 0x4af3,
		  0x90, 0xe1, 0x92, 0x7a, 0x4e, 0x0c, 0x55, 0xa4),
	 
	GUID_INIT(0xefcc06cc, 0x73ac, 0x4bc3,
		  0xbf, 0xf0, 0x76, 0x14, 0x38, 0x07, 0xc3, 0x89),
	 
	GUID_INIT(0xc44d002f, 0x69f9, 0x4e7d,
		  0xa9, 0x04, 0xa7, 0xba, 0xab, 0xdf, 0x43, 0xf7),
	 
	GUID_INIT(0x6c501103, 0xc189, 0x4296,
		  0xba, 0x72, 0x9b, 0xf5, 0xa2, 0x6e, 0xbe, 0x5d),
	 
	GUID_INIT(0x5025030f, 0x842f, 0x4ab4,
		  0xa5, 0x61, 0x99, 0xa5, 0x18, 0x97, 0x62, 0xd0),
};

 
static const guid_t ads_guid =
	GUID_INIT(0xdbb8e3e6, 0x5886, 0x4ba6,
		  0x87, 0x95, 0x13, 0x19, 0xf5, 0x2a, 0x96, 0x6b);

static const guid_t buffer_prop_guid =
	GUID_INIT(0xedb12dd0, 0x363d, 0x4085,
		  0xa3, 0xd2, 0x49, 0x52, 0x2c, 0xa1, 0x60, 0xc4);

static bool acpi_enumerate_nondev_subnodes(acpi_handle scope,
					   union acpi_object *desc,
					   struct acpi_device_data *data,
					   struct fwnode_handle *parent);
static bool acpi_extract_properties(acpi_handle handle,
				    union acpi_object *desc,
				    struct acpi_device_data *data);

static bool acpi_nondev_subnode_extract(union acpi_object *desc,
					acpi_handle handle,
					const union acpi_object *link,
					struct list_head *list,
					struct fwnode_handle *parent)
{
	struct acpi_data_node *dn;
	bool result;

	dn = kzalloc(sizeof(*dn), GFP_KERNEL);
	if (!dn)
		return false;

	dn->name = link->package.elements[0].string.pointer;
	fwnode_init(&dn->fwnode, &acpi_data_fwnode_ops);
	dn->parent = parent;
	INIT_LIST_HEAD(&dn->data.properties);
	INIT_LIST_HEAD(&dn->data.subnodes);

	result = acpi_extract_properties(handle, desc, &dn->data);

	if (handle) {
		acpi_handle scope;
		acpi_status status;

		 
		status = acpi_get_parent(handle, &scope);
		if (ACPI_SUCCESS(status)
		    && acpi_enumerate_nondev_subnodes(scope, desc, &dn->data,
						      &dn->fwnode))
			result = true;
	} else if (acpi_enumerate_nondev_subnodes(NULL, desc, &dn->data,
						  &dn->fwnode)) {
		result = true;
	}

	if (result) {
		dn->handle = handle;
		dn->data.pointer = desc;
		list_add_tail(&dn->sibling, list);
		return true;
	}

	kfree(dn);
	acpi_handle_debug(handle, "Invalid properties/subnodes data, skipping\n");
	return false;
}

static bool acpi_nondev_subnode_data_ok(acpi_handle handle,
					const union acpi_object *link,
					struct list_head *list,
					struct fwnode_handle *parent)
{
	struct acpi_buffer buf = { ACPI_ALLOCATE_BUFFER };
	acpi_status status;

	status = acpi_evaluate_object_typed(handle, NULL, NULL, &buf,
					    ACPI_TYPE_PACKAGE);
	if (ACPI_FAILURE(status))
		return false;

	if (acpi_nondev_subnode_extract(buf.pointer, handle, link, list,
					parent))
		return true;

	ACPI_FREE(buf.pointer);
	return false;
}

static bool acpi_nondev_subnode_ok(acpi_handle scope,
				   const union acpi_object *link,
				   struct list_head *list,
				   struct fwnode_handle *parent)
{
	acpi_handle handle;
	acpi_status status;

	if (!scope)
		return false;

	status = acpi_get_handle(scope, link->package.elements[1].string.pointer,
				 &handle);
	if (ACPI_FAILURE(status))
		return false;

	return acpi_nondev_subnode_data_ok(handle, link, list, parent);
}

static bool acpi_add_nondev_subnodes(acpi_handle scope,
				     union acpi_object *links,
				     struct list_head *list,
				     struct fwnode_handle *parent)
{
	bool ret = false;
	int i;

	for (i = 0; i < links->package.count; i++) {
		union acpi_object *link, *desc;
		acpi_handle handle;
		bool result;

		link = &links->package.elements[i];
		 
		if (link->package.count != 2)
			continue;

		 
		if (link->package.elements[0].type != ACPI_TYPE_STRING)
			continue;

		 
		switch (link->package.elements[1].type) {
		case ACPI_TYPE_STRING:
			result = acpi_nondev_subnode_ok(scope, link, list,
							 parent);
			break;
		case ACPI_TYPE_LOCAL_REFERENCE:
			handle = link->package.elements[1].reference.handle;
			result = acpi_nondev_subnode_data_ok(handle, link, list,
							     parent);
			break;
		case ACPI_TYPE_PACKAGE:
			desc = &link->package.elements[1];
			result = acpi_nondev_subnode_extract(desc, NULL, link,
							     list, parent);
			break;
		default:
			result = false;
			break;
		}
		ret = ret || result;
	}

	return ret;
}

static bool acpi_enumerate_nondev_subnodes(acpi_handle scope,
					   union acpi_object *desc,
					   struct acpi_device_data *data,
					   struct fwnode_handle *parent)
{
	int i;

	 
	for (i = 0; i < desc->package.count; i += 2) {
		const union acpi_object *guid;
		union acpi_object *links;

		guid = &desc->package.elements[i];
		links = &desc->package.elements[i + 1];

		 
		if (guid->type != ACPI_TYPE_BUFFER ||
		    guid->buffer.length != 16 ||
		    links->type != ACPI_TYPE_PACKAGE)
			break;

		if (!guid_equal((guid_t *)guid->buffer.pointer, &ads_guid))
			continue;

		return acpi_add_nondev_subnodes(scope, links, &data->subnodes,
						parent);
	}

	return false;
}

static bool acpi_property_value_ok(const union acpi_object *value)
{
	int j;

	 
	switch (value->type) {
	case ACPI_TYPE_INTEGER:
	case ACPI_TYPE_STRING:
	case ACPI_TYPE_LOCAL_REFERENCE:
		return true;

	case ACPI_TYPE_PACKAGE:
		for (j = 0; j < value->package.count; j++)
			switch (value->package.elements[j].type) {
			case ACPI_TYPE_INTEGER:
			case ACPI_TYPE_STRING:
			case ACPI_TYPE_LOCAL_REFERENCE:
				continue;

			default:
				return false;
			}

		return true;
	}
	return false;
}

static bool acpi_properties_format_valid(const union acpi_object *properties)
{
	int i;

	for (i = 0; i < properties->package.count; i++) {
		const union acpi_object *property;

		property = &properties->package.elements[i];
		 
		if (property->package.count != 2
		    || property->package.elements[0].type != ACPI_TYPE_STRING
		    || !acpi_property_value_ok(&property->package.elements[1]))
			return false;
	}
	return true;
}

static void acpi_init_of_compatible(struct acpi_device *adev)
{
	const union acpi_object *of_compatible;
	int ret;

	ret = acpi_data_get_property_array(&adev->data, "compatible",
					   ACPI_TYPE_STRING, &of_compatible);
	if (ret) {
		ret = acpi_dev_get_property(adev, "compatible",
					    ACPI_TYPE_STRING, &of_compatible);
		if (ret) {
			struct acpi_device *parent;

			parent = acpi_dev_parent(adev);
			if (parent && parent->flags.of_compatible_ok)
				goto out;

			return;
		}
	}
	adev->data.of_compatible = of_compatible;

 out:
	adev->flags.of_compatible_ok = 1;
}

static bool acpi_is_property_guid(const guid_t *guid)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(prp_guids); i++) {
		if (guid_equal(guid, &prp_guids[i]))
			return true;
	}

	return false;
}

struct acpi_device_properties *
acpi_data_add_props(struct acpi_device_data *data, const guid_t *guid,
		    union acpi_object *properties)
{
	struct acpi_device_properties *props;

	props = kzalloc(sizeof(*props), GFP_KERNEL);
	if (props) {
		INIT_LIST_HEAD(&props->list);
		props->guid = guid;
		props->properties = properties;
		list_add_tail(&props->list, &data->properties);
	}

	return props;
}

static void acpi_nondev_subnode_tag(acpi_handle handle, void *context)
{
}

static void acpi_untie_nondev_subnodes(struct acpi_device_data *data)
{
	struct acpi_data_node *dn;

	list_for_each_entry(dn, &data->subnodes, sibling) {
		acpi_detach_data(dn->handle, acpi_nondev_subnode_tag);

		acpi_untie_nondev_subnodes(&dn->data);
	}
}

static bool acpi_tie_nondev_subnodes(struct acpi_device_data *data)
{
	struct acpi_data_node *dn;

	list_for_each_entry(dn, &data->subnodes, sibling) {
		acpi_status status;
		bool ret;

		status = acpi_attach_data(dn->handle, acpi_nondev_subnode_tag, dn);
		if (ACPI_FAILURE(status) && status != AE_ALREADY_EXISTS) {
			acpi_handle_err(dn->handle, "Can't tag data node\n");
			return false;
		}

		ret = acpi_tie_nondev_subnodes(&dn->data);
		if (!ret)
			return ret;
	}

	return true;
}

static void acpi_data_add_buffer_props(acpi_handle handle,
				       struct acpi_device_data *data,
				       union acpi_object *properties)
{
	struct acpi_device_properties *props;
	union acpi_object *package;
	size_t alloc_size;
	unsigned int i;
	u32 *count;

	if (check_mul_overflow((size_t)properties->package.count,
			       sizeof(*package) + sizeof(void *),
			       &alloc_size) ||
	    check_add_overflow(sizeof(*props) + sizeof(*package), alloc_size,
			       &alloc_size)) {
		acpi_handle_warn(handle,
				 "can't allocate memory for %u buffer props",
				 properties->package.count);
		return;
	}

	props = kvzalloc(alloc_size, GFP_KERNEL);
	if (!props)
		return;

	props->guid = &buffer_prop_guid;
	props->bufs = (void *)(props + 1);
	props->properties = (void *)(props->bufs + properties->package.count);

	 
	package = props->properties;
	package->type = ACPI_TYPE_PACKAGE;
	package->package.elements = package + 1;
	count = &package->package.count;
	*count = 0;

	 
	package++;

	for (i = 0; i < properties->package.count; i++) {
		struct acpi_buffer buf = { ACPI_ALLOCATE_BUFFER };
		union acpi_object *property = &properties->package.elements[i];
		union acpi_object *prop, *obj, *buf_obj;
		acpi_status status;

		if (property->type != ACPI_TYPE_PACKAGE ||
		    property->package.count != 2) {
			acpi_handle_warn(handle,
					 "buffer property %u has %u entries\n",
					 i, property->package.count);
			continue;
		}

		prop = &property->package.elements[0];
		obj = &property->package.elements[1];

		if (prop->type != ACPI_TYPE_STRING ||
		    obj->type != ACPI_TYPE_STRING) {
			acpi_handle_warn(handle,
					 "wrong object types %u and %u\n",
					 prop->type, obj->type);
			continue;
		}

		status = acpi_evaluate_object_typed(handle, obj->string.pointer,
						    NULL, &buf,
						    ACPI_TYPE_BUFFER);
		if (ACPI_FAILURE(status)) {
			acpi_handle_warn(handle,
					 "can't evaluate \"%*pE\" as buffer\n",
					 obj->string.length,
					 obj->string.pointer);
			continue;
		}

		package->type = ACPI_TYPE_PACKAGE;
		package->package.elements = prop;
		package->package.count = 2;

		buf_obj = buf.pointer;

		 
		obj->type = ACPI_TYPE_BUFFER;
		obj->buffer.length = buf_obj->buffer.length;
		obj->buffer.pointer = buf_obj->buffer.pointer;

		props->bufs[i] = buf.pointer;
		package++;
		(*count)++;
	}

	if (*count)
		list_add(&props->list, &data->properties);
	else
		kvfree(props);
}

static bool acpi_extract_properties(acpi_handle scope, union acpi_object *desc,
				    struct acpi_device_data *data)
{
	int i;

	if (desc->package.count % 2)
		return false;

	 
	for (i = 0; i < desc->package.count; i += 2) {
		const union acpi_object *guid;
		union acpi_object *properties;

		guid = &desc->package.elements[i];
		properties = &desc->package.elements[i + 1];

		 
		if (guid->type != ACPI_TYPE_BUFFER ||
		    guid->buffer.length != 16 ||
		    properties->type != ACPI_TYPE_PACKAGE)
			break;

		if (guid_equal((guid_t *)guid->buffer.pointer,
			       &buffer_prop_guid)) {
			acpi_data_add_buffer_props(scope, data, properties);
			continue;
		}

		if (!acpi_is_property_guid((guid_t *)guid->buffer.pointer))
			continue;

		 
		if (!acpi_properties_format_valid(properties))
			continue;

		acpi_data_add_props(data, (const guid_t *)guid->buffer.pointer,
				    properties);
	}

	return !list_empty(&data->properties);
}

void acpi_init_properties(struct acpi_device *adev)
{
	struct acpi_buffer buf = { ACPI_ALLOCATE_BUFFER };
	struct acpi_hardware_id *hwid;
	acpi_status status;
	bool acpi_of = false;

	INIT_LIST_HEAD(&adev->data.properties);
	INIT_LIST_HEAD(&adev->data.subnodes);

	if (!adev->handle)
		return;

	 
	list_for_each_entry(hwid, &adev->pnp.ids, list) {
		if (!strcmp(hwid->id, ACPI_DT_NAMESPACE_HID)) {
			acpi_of = true;
			break;
		}
	}

	status = acpi_evaluate_object_typed(adev->handle, "_DSD", NULL, &buf,
					    ACPI_TYPE_PACKAGE);
	if (ACPI_FAILURE(status))
		goto out;

	if (acpi_extract_properties(adev->handle, buf.pointer, &adev->data)) {
		adev->data.pointer = buf.pointer;
		if (acpi_of)
			acpi_init_of_compatible(adev);
	}
	if (acpi_enumerate_nondev_subnodes(adev->handle, buf.pointer,
					&adev->data, acpi_fwnode_handle(adev)))
		adev->data.pointer = buf.pointer;

	if (!adev->data.pointer) {
		acpi_handle_debug(adev->handle, "Invalid _DSD data, skipping\n");
		ACPI_FREE(buf.pointer);
	} else {
		if (!acpi_tie_nondev_subnodes(&adev->data))
			acpi_untie_nondev_subnodes(&adev->data);
	}

 out:
	if (acpi_of && !adev->flags.of_compatible_ok)
		acpi_handle_info(adev->handle,
			 ACPI_DT_NAMESPACE_HID " requires 'compatible' property\n");

	if (!adev->data.pointer)
		acpi_extract_apple_properties(adev);
}

static void acpi_free_device_properties(struct list_head *list)
{
	struct acpi_device_properties *props, *tmp;

	list_for_each_entry_safe(props, tmp, list, list) {
		u32 i;

		list_del(&props->list);
		 
		if (props->bufs)
			for (i = 0; i < props->properties->package.count; i++)
				ACPI_FREE(props->bufs[i]);
		kvfree(props);
	}
}

static void acpi_destroy_nondev_subnodes(struct list_head *list)
{
	struct acpi_data_node *dn, *next;

	if (list_empty(list))
		return;

	list_for_each_entry_safe_reverse(dn, next, list, sibling) {
		acpi_destroy_nondev_subnodes(&dn->data.subnodes);
		wait_for_completion(&dn->kobj_done);
		list_del(&dn->sibling);
		ACPI_FREE((void *)dn->data.pointer);
		acpi_free_device_properties(&dn->data.properties);
		kfree(dn);
	}
}

void acpi_free_properties(struct acpi_device *adev)
{
	acpi_untie_nondev_subnodes(&adev->data);
	acpi_destroy_nondev_subnodes(&adev->data.subnodes);
	ACPI_FREE((void *)adev->data.pointer);
	adev->data.of_compatible = NULL;
	adev->data.pointer = NULL;
	acpi_free_device_properties(&adev->data.properties);
}

 
static int acpi_data_get_property(const struct acpi_device_data *data,
				  const char *name, acpi_object_type type,
				  const union acpi_object **obj)
{
	const struct acpi_device_properties *props;

	if (!data || !name)
		return -EINVAL;

	if (!data->pointer || list_empty(&data->properties))
		return -EINVAL;

	list_for_each_entry(props, &data->properties, list) {
		const union acpi_object *properties;
		unsigned int i;

		properties = props->properties;
		for (i = 0; i < properties->package.count; i++) {
			const union acpi_object *propname, *propvalue;
			const union acpi_object *property;

			property = &properties->package.elements[i];

			propname = &property->package.elements[0];
			propvalue = &property->package.elements[1];

			if (!strcmp(name, propname->string.pointer)) {
				if (type != ACPI_TYPE_ANY &&
				    propvalue->type != type)
					return -EPROTO;
				if (obj)
					*obj = propvalue;

				return 0;
			}
		}
	}
	return -EINVAL;
}

 
int acpi_dev_get_property(const struct acpi_device *adev, const char *name,
			  acpi_object_type type, const union acpi_object **obj)
{
	return adev ? acpi_data_get_property(&adev->data, name, type, obj) : -EINVAL;
}
EXPORT_SYMBOL_GPL(acpi_dev_get_property);

static const struct acpi_device_data *
acpi_device_data_of_node(const struct fwnode_handle *fwnode)
{
	if (is_acpi_device_node(fwnode)) {
		const struct acpi_device *adev = to_acpi_device_node(fwnode);
		return &adev->data;
	}
	if (is_acpi_data_node(fwnode)) {
		const struct acpi_data_node *dn = to_acpi_data_node(fwnode);
		return &dn->data;
	}
	return NULL;
}

 
int acpi_node_prop_get(const struct fwnode_handle *fwnode,
		       const char *propname, void **valptr)
{
	return acpi_data_get_property(acpi_device_data_of_node(fwnode),
				      propname, ACPI_TYPE_ANY,
				      (const union acpi_object **)valptr);
}

 
static int acpi_data_get_property_array(const struct acpi_device_data *data,
					const char *name,
					acpi_object_type type,
					const union acpi_object **obj)
{
	const union acpi_object *prop;
	int ret, i;

	ret = acpi_data_get_property(data, name, ACPI_TYPE_PACKAGE, &prop);
	if (ret)
		return ret;

	if (type != ACPI_TYPE_ANY) {
		 
		for (i = 0; i < prop->package.count; i++)
			if (prop->package.elements[i].type != type)
				return -EPROTO;
	}
	if (obj)
		*obj = prop;

	return 0;
}

static struct fwnode_handle *
acpi_fwnode_get_named_child_node(const struct fwnode_handle *fwnode,
				 const char *childname)
{
	struct fwnode_handle *child;

	fwnode_for_each_child_node(fwnode, child) {
		if (is_acpi_data_node(child)) {
			if (acpi_data_node_match(child, childname))
				return child;
			continue;
		}

		if (!strncmp(acpi_device_bid(to_acpi_device_node(child)),
			     childname, ACPI_NAMESEG_SIZE))
			return child;
	}

	return NULL;
}

static int acpi_get_ref_args(struct fwnode_reference_args *args,
			     struct fwnode_handle *ref_fwnode,
			     const union acpi_object **element,
			     const union acpi_object *end, size_t num_args)
{
	u32 nargs = 0, i;

	 
	for (; *element < end && (*element)->type == ACPI_TYPE_STRING;
	     (*element)++) {
		const char *child_name = (*element)->string.pointer;

		ref_fwnode = acpi_fwnode_get_named_child_node(ref_fwnode, child_name);
		if (!ref_fwnode)
			return -EINVAL;
	}

	 
	for (i = 0; (*element) + i < end && i < num_args; i++) {
		acpi_object_type type = (*element)[i].type;

		if (type == ACPI_TYPE_LOCAL_REFERENCE)
			break;

		if (type == ACPI_TYPE_INTEGER)
			nargs++;
		else
			return -EINVAL;
	}

	if (nargs > NR_FWNODE_REFERENCE_ARGS)
		return -EINVAL;

	if (args) {
		args->fwnode = ref_fwnode;
		args->nargs = nargs;
		for (i = 0; i < nargs; i++)
			args->args[i] = (*element)[i].integer.value;
	}

	(*element) += nargs;

	return 0;
}

 
int __acpi_node_get_property_reference(const struct fwnode_handle *fwnode,
	const char *propname, size_t index, size_t num_args,
	struct fwnode_reference_args *args)
{
	const union acpi_object *element, *end;
	const union acpi_object *obj;
	const struct acpi_device_data *data;
	struct acpi_device *device;
	int ret, idx = 0;

	data = acpi_device_data_of_node(fwnode);
	if (!data)
		return -ENOENT;

	ret = acpi_data_get_property(data, propname, ACPI_TYPE_ANY, &obj);
	if (ret)
		return ret == -EINVAL ? -ENOENT : -EINVAL;

	switch (obj->type) {
	case ACPI_TYPE_LOCAL_REFERENCE:
		 
		if (index)
			return -ENOENT;

		device = acpi_fetch_acpi_dev(obj->reference.handle);
		if (!device)
			return -EINVAL;

		if (!args)
			return 0;

		args->fwnode = acpi_fwnode_handle(device);
		args->nargs = 0;
		return 0;
	case ACPI_TYPE_PACKAGE:
		 
		break;
	default:
		return -EINVAL;
	}

	if (index >= obj->package.count)
		return -ENOENT;

	element = obj->package.elements;
	end = element + obj->package.count;

	while (element < end) {
		switch (element->type) {
		case ACPI_TYPE_LOCAL_REFERENCE:
			device = acpi_fetch_acpi_dev(element->reference.handle);
			if (!device)
				return -EINVAL;

			element++;

			ret = acpi_get_ref_args(idx == index ? args : NULL,
						acpi_fwnode_handle(device),
						&element, end, num_args);
			if (ret < 0)
				return ret;

			if (idx == index)
				return 0;

			break;
		case ACPI_TYPE_INTEGER:
			if (idx == index)
				return -ENOENT;
			element++;
			break;
		default:
			return -EINVAL;
		}

		idx++;
	}

	return -ENOENT;
}
EXPORT_SYMBOL_GPL(__acpi_node_get_property_reference);

static int acpi_data_prop_read_single(const struct acpi_device_data *data,
				      const char *propname,
				      enum dev_prop_type proptype, void *val)
{
	const union acpi_object *obj;
	int ret = 0;

	if (proptype >= DEV_PROP_U8 && proptype <= DEV_PROP_U64)
		ret = acpi_data_get_property(data, propname, ACPI_TYPE_INTEGER, &obj);
	else if (proptype == DEV_PROP_STRING)
		ret = acpi_data_get_property(data, propname, ACPI_TYPE_STRING, &obj);
	if (ret)
		return ret;

	switch (proptype) {
	case DEV_PROP_U8:
		if (obj->integer.value > U8_MAX)
			return -EOVERFLOW;
		if (val)
			*(u8 *)val = obj->integer.value;
		break;
	case DEV_PROP_U16:
		if (obj->integer.value > U16_MAX)
			return -EOVERFLOW;
		if (val)
			*(u16 *)val = obj->integer.value;
		break;
	case DEV_PROP_U32:
		if (obj->integer.value > U32_MAX)
			return -EOVERFLOW;
		if (val)
			*(u32 *)val = obj->integer.value;
		break;
	case DEV_PROP_U64:
		if (val)
			*(u64 *)val = obj->integer.value;
		break;
	case DEV_PROP_STRING:
		if (val)
			*(char **)val = obj->string.pointer;
		return 1;
	default:
		return -EINVAL;
	}

	 
	return val ? 0 : 1;
}

#define acpi_copy_property_array_uint(items, val, nval)			\
	({								\
		typeof(items) __items = items;				\
		typeof(val) __val = val;				\
		typeof(nval) __nval = nval;				\
		size_t i;						\
		int ret = 0;						\
									\
		for (i = 0; i < __nval; i++) {				\
			if (__items->type == ACPI_TYPE_BUFFER) {	\
				__val[i] = __items->buffer.pointer[i];	\
				continue;				\
			}						\
			if (__items[i].type != ACPI_TYPE_INTEGER) {	\
				ret = -EPROTO;				\
				break;					\
			}						\
			if (__items[i].integer.value > _Generic(__val,	\
								u8 *: U8_MAX, \
								u16 *: U16_MAX, \
								u32 *: U32_MAX, \
								u64 *: U64_MAX)) { \
				ret = -EOVERFLOW;			\
				break;					\
			}						\
									\
			__val[i] = __items[i].integer.value;		\
		}							\
		ret;							\
	})

static int acpi_copy_property_array_string(const union acpi_object *items,
					   char **val, size_t nval)
{
	int i;

	for (i = 0; i < nval; i++) {
		if (items[i].type != ACPI_TYPE_STRING)
			return -EPROTO;

		val[i] = items[i].string.pointer;
	}
	return nval;
}

static int acpi_data_prop_read(const struct acpi_device_data *data,
			       const char *propname,
			       enum dev_prop_type proptype,
			       void *val, size_t nval)
{
	const union acpi_object *obj;
	const union acpi_object *items;
	int ret;

	if (nval == 1 || !val) {
		ret = acpi_data_prop_read_single(data, propname, proptype, val);
		 
		if (ret >= 0 || ret == -EOVERFLOW)
			return ret;

		 
	}

	ret = acpi_data_get_property_array(data, propname, ACPI_TYPE_ANY, &obj);
	if (ret && proptype >= DEV_PROP_U8 && proptype <= DEV_PROP_U64)
		ret = acpi_data_get_property(data, propname, ACPI_TYPE_BUFFER,
					     &obj);
	if (ret)
		return ret;

	if (!val) {
		if (obj->type == ACPI_TYPE_BUFFER)
			return obj->buffer.length;

		return obj->package.count;
	}

	switch (proptype) {
	case DEV_PROP_STRING:
		break;
	default:
		if (obj->type == ACPI_TYPE_BUFFER) {
			if (nval > obj->buffer.length)
				return -EOVERFLOW;
		} else {
			if (nval > obj->package.count)
				return -EOVERFLOW;
		}
		break;
	}
	if (nval == 0)
		return -EINVAL;

	if (obj->type == ACPI_TYPE_BUFFER) {
		if (proptype != DEV_PROP_U8)
			return -EPROTO;
		items = obj;
	} else {
		items = obj->package.elements;
	}

	switch (proptype) {
	case DEV_PROP_U8:
		ret = acpi_copy_property_array_uint(items, (u8 *)val, nval);
		break;
	case DEV_PROP_U16:
		ret = acpi_copy_property_array_uint(items, (u16 *)val, nval);
		break;
	case DEV_PROP_U32:
		ret = acpi_copy_property_array_uint(items, (u32 *)val, nval);
		break;
	case DEV_PROP_U64:
		ret = acpi_copy_property_array_uint(items, (u64 *)val, nval);
		break;
	case DEV_PROP_STRING:
		ret = acpi_copy_property_array_string(
			items, (char **)val,
			min_t(u32, nval, obj->package.count));
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

 
static int acpi_node_prop_read(const struct fwnode_handle *fwnode,
			       const char *propname, enum dev_prop_type proptype,
			       void *val, size_t nval)
{
	return acpi_data_prop_read(acpi_device_data_of_node(fwnode),
				   propname, proptype, val, nval);
}

static int stop_on_next(struct acpi_device *adev, void *data)
{
	struct acpi_device **ret_p = data;

	if (!*ret_p) {
		*ret_p = adev;
		return 1;
	}

	 
	if (*ret_p == adev)
		*ret_p = NULL;

	return 0;
}

 
struct fwnode_handle *acpi_get_next_subnode(const struct fwnode_handle *fwnode,
					    struct fwnode_handle *child)
{
	struct acpi_device *adev = to_acpi_device_node(fwnode);

	if ((!child || is_acpi_device_node(child)) && adev) {
		struct acpi_device *child_adev = to_acpi_device_node(child);

		acpi_dev_for_each_child(adev, stop_on_next, &child_adev);
		if (child_adev)
			return acpi_fwnode_handle(child_adev);

		child = NULL;
	}

	if (!child || is_acpi_data_node(child)) {
		const struct acpi_data_node *data = to_acpi_data_node(fwnode);
		const struct list_head *head;
		struct list_head *next;
		struct acpi_data_node *dn;

		 
		adev = to_acpi_device_node(fwnode);
		if (adev)
			head = &adev->data.subnodes;
		else if (data)
			head = &data->data.subnodes;
		else
			return NULL;

		if (list_empty(head))
			return NULL;

		if (child) {
			dn = to_acpi_data_node(child);
			next = dn->sibling.next;
			if (next == head)
				return NULL;

			dn = list_entry(next, struct acpi_data_node, sibling);
		} else {
			dn = list_first_entry(head, struct acpi_data_node, sibling);
		}
		return &dn->fwnode;
	}
	return NULL;
}

 
static struct fwnode_handle *
acpi_node_get_parent(const struct fwnode_handle *fwnode)
{
	if (is_acpi_data_node(fwnode)) {
		 
		return to_acpi_data_node(fwnode)->parent;
	}
	if (is_acpi_device_node(fwnode)) {
		struct acpi_device *parent;

		parent = acpi_dev_parent(to_acpi_device_node(fwnode));
		if (parent)
			return acpi_fwnode_handle(parent);
	}

	return NULL;
}

 
static bool is_acpi_graph_node(struct fwnode_handle *fwnode,
			       const char *str)
{
	unsigned int len = strlen(str);
	const char *name;

	if (!len || !is_acpi_data_node(fwnode))
		return false;

	name = to_acpi_data_node(fwnode)->name;

	return (fwnode_property_present(fwnode, "reg") &&
		!strncmp(name, str, len) && name[len] == '@') ||
		fwnode_property_present(fwnode, str);
}

 
static struct fwnode_handle *acpi_graph_get_next_endpoint(
	const struct fwnode_handle *fwnode, struct fwnode_handle *prev)
{
	struct fwnode_handle *port = NULL;
	struct fwnode_handle *endpoint;

	if (!prev) {
		do {
			port = fwnode_get_next_child_node(fwnode, port);
			 
			if (is_acpi_graph_node(port, "port"))
				break;
		} while (port);
	} else {
		port = fwnode_get_parent(prev);
	}

	if (!port)
		return NULL;

	endpoint = fwnode_get_next_child_node(port, prev);
	while (!endpoint) {
		port = fwnode_get_next_child_node(fwnode, port);
		if (!port)
			break;
		if (is_acpi_graph_node(port, "port"))
			endpoint = fwnode_get_next_child_node(port, NULL);
	}

	 
	if (!is_acpi_graph_node(endpoint, "endpoint"))
		return NULL;

	return endpoint;
}

 
static struct fwnode_handle *acpi_graph_get_child_prop_value(
	const struct fwnode_handle *fwnode, const char *prop_name,
	unsigned int val)
{
	struct fwnode_handle *child;

	fwnode_for_each_child_node(fwnode, child) {
		u32 nr;

		if (fwnode_property_read_u32(child, prop_name, &nr))
			continue;

		if (val == nr)
			return child;
	}

	return NULL;
}


 
static struct fwnode_handle *
acpi_graph_get_remote_endpoint(const struct fwnode_handle *__fwnode)
{
	struct fwnode_handle *fwnode;
	unsigned int port_nr, endpoint_nr;
	struct fwnode_reference_args args;
	int ret;

	memset(&args, 0, sizeof(args));
	ret = acpi_node_get_property_reference(__fwnode, "remote-endpoint", 0,
					       &args);
	if (ret)
		return NULL;

	 
	if (!is_acpi_device_node(args.fwnode))
		return args.nargs ? NULL : args.fwnode;

	 
	if (args.nargs != 2)
		return NULL;

	fwnode = args.fwnode;
	port_nr = args.args[0];
	endpoint_nr = args.args[1];

	fwnode = acpi_graph_get_child_prop_value(fwnode, "port", port_nr);

	return acpi_graph_get_child_prop_value(fwnode, "endpoint", endpoint_nr);
}

static bool acpi_fwnode_device_is_available(const struct fwnode_handle *fwnode)
{
	if (!is_acpi_device_node(fwnode))
		return false;

	return acpi_device_is_present(to_acpi_device_node(fwnode));
}

static const void *
acpi_fwnode_device_get_match_data(const struct fwnode_handle *fwnode,
				  const struct device *dev)
{
	return acpi_device_get_match_data(dev);
}

static bool acpi_fwnode_device_dma_supported(const struct fwnode_handle *fwnode)
{
	return acpi_dma_supported(to_acpi_device_node(fwnode));
}

static enum dev_dma_attr
acpi_fwnode_device_get_dma_attr(const struct fwnode_handle *fwnode)
{
	return acpi_get_dma_attr(to_acpi_device_node(fwnode));
}

static bool acpi_fwnode_property_present(const struct fwnode_handle *fwnode,
					 const char *propname)
{
	return !acpi_node_prop_get(fwnode, propname, NULL);
}

static int
acpi_fwnode_property_read_int_array(const struct fwnode_handle *fwnode,
				    const char *propname,
				    unsigned int elem_size, void *val,
				    size_t nval)
{
	enum dev_prop_type type;

	switch (elem_size) {
	case sizeof(u8):
		type = DEV_PROP_U8;
		break;
	case sizeof(u16):
		type = DEV_PROP_U16;
		break;
	case sizeof(u32):
		type = DEV_PROP_U32;
		break;
	case sizeof(u64):
		type = DEV_PROP_U64;
		break;
	default:
		return -ENXIO;
	}

	return acpi_node_prop_read(fwnode, propname, type, val, nval);
}

static int
acpi_fwnode_property_read_string_array(const struct fwnode_handle *fwnode,
				       const char *propname, const char **val,
				       size_t nval)
{
	return acpi_node_prop_read(fwnode, propname, DEV_PROP_STRING,
				   val, nval);
}

static int
acpi_fwnode_get_reference_args(const struct fwnode_handle *fwnode,
			       const char *prop, const char *nargs_prop,
			       unsigned int args_count, unsigned int index,
			       struct fwnode_reference_args *args)
{
	return __acpi_node_get_property_reference(fwnode, prop, index,
						  args_count, args);
}

static const char *acpi_fwnode_get_name(const struct fwnode_handle *fwnode)
{
	const struct acpi_device *adev;
	struct fwnode_handle *parent;

	 
	parent = fwnode_get_parent(fwnode);
	if (!parent)
		return "\\";

	fwnode_handle_put(parent);

	if (is_acpi_data_node(fwnode)) {
		const struct acpi_data_node *dn = to_acpi_data_node(fwnode);

		return dn->name;
	}

	adev = to_acpi_device_node(fwnode);
	if (WARN_ON(!adev))
		return NULL;

	return acpi_device_bid(adev);
}

static const char *
acpi_fwnode_get_name_prefix(const struct fwnode_handle *fwnode)
{
	struct fwnode_handle *parent;

	 
	parent = fwnode_get_parent(fwnode);
	if (!parent)
		return "";

	 
	parent = fwnode_get_next_parent(parent);
	if (!parent)
		return "";

	fwnode_handle_put(parent);

	 
	return ".";
}

static struct fwnode_handle *
acpi_fwnode_get_parent(struct fwnode_handle *fwnode)
{
	return acpi_node_get_parent(fwnode);
}

static int acpi_fwnode_graph_parse_endpoint(const struct fwnode_handle *fwnode,
					    struct fwnode_endpoint *endpoint)
{
	struct fwnode_handle *port_fwnode = fwnode_get_parent(fwnode);

	endpoint->local_fwnode = fwnode;

	if (fwnode_property_read_u32(port_fwnode, "reg", &endpoint->port))
		fwnode_property_read_u32(port_fwnode, "port", &endpoint->port);
	if (fwnode_property_read_u32(fwnode, "reg", &endpoint->id))
		fwnode_property_read_u32(fwnode, "endpoint", &endpoint->id);

	return 0;
}

static int acpi_fwnode_irq_get(const struct fwnode_handle *fwnode,
			       unsigned int index)
{
	struct resource res;
	int ret;

	ret = acpi_irq_get(ACPI_HANDLE_FWNODE(fwnode), index, &res);
	if (ret)
		return ret;

	return res.start;
}

#define DECLARE_ACPI_FWNODE_OPS(ops) \
	const struct fwnode_operations ops = {				\
		.device_is_available = acpi_fwnode_device_is_available, \
		.device_get_match_data = acpi_fwnode_device_get_match_data, \
		.device_dma_supported =				\
			acpi_fwnode_device_dma_supported,		\
		.device_get_dma_attr = acpi_fwnode_device_get_dma_attr,	\
		.property_present = acpi_fwnode_property_present,	\
		.property_read_int_array =				\
			acpi_fwnode_property_read_int_array,		\
		.property_read_string_array =				\
			acpi_fwnode_property_read_string_array,		\
		.get_parent = acpi_node_get_parent,			\
		.get_next_child_node = acpi_get_next_subnode,		\
		.get_named_child_node = acpi_fwnode_get_named_child_node, \
		.get_name = acpi_fwnode_get_name,			\
		.get_name_prefix = acpi_fwnode_get_name_prefix,		\
		.get_reference_args = acpi_fwnode_get_reference_args,	\
		.graph_get_next_endpoint =				\
			acpi_graph_get_next_endpoint,			\
		.graph_get_remote_endpoint =				\
			acpi_graph_get_remote_endpoint,			\
		.graph_get_port_parent = acpi_fwnode_get_parent,	\
		.graph_parse_endpoint = acpi_fwnode_graph_parse_endpoint, \
		.irq_get = acpi_fwnode_irq_get,				\
	};								\
	EXPORT_SYMBOL_GPL(ops)

DECLARE_ACPI_FWNODE_OPS(acpi_device_fwnode_ops);
DECLARE_ACPI_FWNODE_OPS(acpi_data_fwnode_ops);
const struct fwnode_operations acpi_static_fwnode_ops;

bool is_acpi_device_node(const struct fwnode_handle *fwnode)
{
	return !IS_ERR_OR_NULL(fwnode) &&
		fwnode->ops == &acpi_device_fwnode_ops;
}
EXPORT_SYMBOL(is_acpi_device_node);

bool is_acpi_data_node(const struct fwnode_handle *fwnode)
{
	return !IS_ERR_OR_NULL(fwnode) && fwnode->ops == &acpi_data_fwnode_ops;
}
EXPORT_SYMBOL(is_acpi_data_node);
