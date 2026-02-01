

 

#include <linux/vmalloc.h>
#include <uapi/drm/habanalabs_accel.h>
#include "habanalabs.h"

 
char *hl_format_as_binary(char *buf, size_t buf_len, u32 n)
{
	int i;
	u32 bit;
	bool leading0 = true;
	char *wrptr = buf;

	if (buf_len > 0 && buf_len < 3) {
		*wrptr = '\0';
		return buf;
	}

	wrptr[0] = '0';
	wrptr[1] = 'b';
	wrptr += 2;
	 
	buf_len -= 3;

	for (i = 0; i < sizeof(n) * BITS_PER_BYTE && buf_len; ++i, n <<= 1) {
		 
		bit = n & (1 << (sizeof(n) * BITS_PER_BYTE - 1));
		bit = !!bit;
		leading0 &= !bit;
		if (!leading0) {
			*wrptr = '0' + bit;
			++wrptr;
		}
	}

	*wrptr = '\0';

	return buf;
}

 
static int resize_to_fit(char **buf, size_t *size, size_t desired_size)
{
	char *resized_buf;
	size_t new_size;

	if (*size >= desired_size)
		return 0;

	 
	new_size = max_t(size_t, PAGE_SIZE, round_up(desired_size, PAGE_SIZE));
	resized_buf = vmalloc(new_size);
	if (!resized_buf)
		return -ENOMEM;
	memcpy(resized_buf, *buf, *size);
	vfree(*buf);
	*buf = resized_buf;
	*size = new_size;

	return 1;
}

 
int hl_snprintf_resize(char **buf, size_t *size, size_t *offset,
			   const char *format, ...)
{
	va_list args;
	size_t length;
	int rc;

	if (*buf == NULL && (*size != 0 || *offset != 0))
		return -EINVAL;

	va_start(args, format);
	length = vsnprintf(*buf + *offset, *size - *offset, format, args);
	va_end(args);

	rc = resize_to_fit(buf, size, *offset + length + 1);
	if (rc < 0)
		return rc;
	else if (rc > 0) {
		 
		va_start(args, format);
		length = vsnprintf(*buf + *offset, *size - *offset, format,
				   args);
		va_end(args);
	}

	*offset += length;

	return 0;
}

 
const char *hl_sync_engine_to_string(enum hl_sync_engine_type engine_type)
{
	switch (engine_type) {
	case ENGINE_DMA:
		return "DMA";
	case ENGINE_MME:
		return "MME";
	case ENGINE_TPC:
		return "TPC";
	}
	return "Invalid Engine Type";
}

 
static int hl_print_resize_sync_engine(char **buf, size_t *size, size_t *offset,
				enum hl_sync_engine_type engine_type,
				u32 engine_id)
{
	return hl_snprintf_resize(buf, size, offset, "%s%u",
			hl_sync_engine_to_string(engine_type), engine_id);
}

 
const char *hl_state_dump_get_sync_name(struct hl_device *hdev, u32 sync_id)
{
	struct hl_state_dump_specs *sds = &hdev->state_dump_specs;
	struct hl_hw_obj_name_entry *entry;

	hash_for_each_possible(sds->so_id_to_str_tb, entry,
				node, sync_id)
		if (sync_id == entry->id)
			return entry->name;

	return NULL;
}

 
const char *hl_state_dump_get_monitor_name(struct hl_device *hdev,
					struct hl_mon_state_dump *mon)
{
	struct hl_state_dump_specs *sds = &hdev->state_dump_specs;
	struct hl_hw_obj_name_entry *entry;

	hash_for_each_possible(sds->monitor_id_to_str_tb,
				entry, node, mon->id)
		if (mon->id == entry->id)
			return entry->name;

	return NULL;
}

 
void hl_state_dump_free_sync_to_engine_map(struct hl_sync_to_engine_map *map)
{
	struct hl_sync_to_engine_map_entry *entry;
	struct hlist_node *tmp_node;
	int i;

	hash_for_each_safe(map->tb, i, tmp_node, entry, node) {
		hash_del(&entry->node);
		kfree(entry);
	}
}

 
static struct hl_sync_to_engine_map_entry *
hl_state_dump_get_sync_to_engine(struct hl_sync_to_engine_map *map, u32 sync_id)
{
	struct hl_sync_to_engine_map_entry *entry;

	hash_for_each_possible(map->tb, entry, node, sync_id)
		if (entry->sync_id == sync_id)
			return entry;
	return NULL;
}

 
static u32 *hl_state_dump_read_sync_objects(struct hl_device *hdev, u32 index)
{
	struct hl_state_dump_specs *sds = &hdev->state_dump_specs;
	u32 *sync_objects;
	s64 base_addr;  
	int i;

	base_addr = sds->props[SP_SYNC_OBJ_BASE_ADDR] +
			sds->props[SP_NEXT_SYNC_OBJ_ADDR] * index;

	sync_objects = vmalloc(sds->props[SP_SYNC_OBJ_AMOUNT] * sizeof(u32));
	if (!sync_objects)
		return NULL;

	for (i = 0; i < sds->props[SP_SYNC_OBJ_AMOUNT]; ++i)
		sync_objects[i] = RREG32(base_addr + i * sizeof(u32));

	return sync_objects;
}

 
static void hl_state_dump_free_sync_objects(u32 *sync_objects)
{
	vfree(sync_objects);
}


 
static int
hl_state_dump_print_syncs_single_block(struct hl_device *hdev, u32 index,
				char **buf, size_t *size, size_t *offset,
				struct hl_sync_to_engine_map *map)
{
	struct hl_state_dump_specs *sds = &hdev->state_dump_specs;
	const char *sync_name;
	u32 *sync_objects = NULL;
	int rc = 0, i;

	if (sds->sync_namager_names) {
		rc = hl_snprintf_resize(
			buf, size, offset, "%s\n",
			sds->sync_namager_names[index]);
		if (rc)
			goto out;
	}

	sync_objects = hl_state_dump_read_sync_objects(hdev, index);
	if (!sync_objects) {
		rc = -ENOMEM;
		goto out;
	}

	for (i = 0; i < sds->props[SP_SYNC_OBJ_AMOUNT]; ++i) {
		struct hl_sync_to_engine_map_entry *entry;
		u64 sync_object_addr;

		if (!sync_objects[i])
			continue;

		sync_object_addr = sds->props[SP_SYNC_OBJ_BASE_ADDR] +
				sds->props[SP_NEXT_SYNC_OBJ_ADDR] * index +
				i * sizeof(u32);

		rc = hl_snprintf_resize(buf, size, offset, "sync id: %u", i);
		if (rc)
			goto free_sync_objects;
		sync_name = hl_state_dump_get_sync_name(hdev, i);
		if (sync_name) {
			rc = hl_snprintf_resize(buf, size, offset, " %s",
						sync_name);
			if (rc)
				goto free_sync_objects;
		}
		rc = hl_snprintf_resize(buf, size, offset, ", value: %u",
					sync_objects[i]);
		if (rc)
			goto free_sync_objects;

		 
		entry = hl_state_dump_get_sync_to_engine(map,
			(u32)sync_object_addr);
		if (entry) {
			rc = hl_snprintf_resize(buf, size, offset,
						", Engine: ");
			if (rc)
				goto free_sync_objects;
			rc = hl_print_resize_sync_engine(buf, size, offset,
						entry->engine_type,
						entry->engine_id);
			if (rc)
				goto free_sync_objects;
		}

		rc = hl_snprintf_resize(buf, size, offset, "\n");
		if (rc)
			goto free_sync_objects;
	}

free_sync_objects:
	hl_state_dump_free_sync_objects(sync_objects);
out:
	return rc;
}

 
static int hl_state_dump_print_syncs(struct hl_device *hdev,
					char **buf, size_t *size,
					size_t *offset)

{
	struct hl_state_dump_specs *sds = &hdev->state_dump_specs;
	struct hl_sync_to_engine_map *map;
	u32 index;
	int rc = 0;

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return -ENOMEM;

	rc = sds->funcs.gen_sync_to_engine_map(hdev, map);
	if (rc)
		goto free_map_mem;

	rc = hl_snprintf_resize(buf, size, offset, "Non zero sync objects:\n");
	if (rc)
		goto out;

	if (sds->sync_namager_names) {
		for (index = 0; sds->sync_namager_names[index]; ++index) {
			rc = hl_state_dump_print_syncs_single_block(
				hdev, index, buf, size, offset, map);
			if (rc)
				goto out;
		}
	} else {
		for (index = 0; index < sds->props[SP_NUM_CORES]; ++index) {
			rc = hl_state_dump_print_syncs_single_block(
				hdev, index, buf, size, offset, map);
			if (rc)
				goto out;
		}
	}

out:
	hl_state_dump_free_sync_to_engine_map(map);
free_map_mem:
	kfree(map);

	return rc;
}

 
static struct hl_mon_state_dump *
hl_state_dump_alloc_read_sm_block_monitors(struct hl_device *hdev, u32 index)
{
	struct hl_state_dump_specs *sds = &hdev->state_dump_specs;
	struct hl_mon_state_dump *monitors;
	s64 base_addr;  
	int i;

	monitors = vmalloc(sds->props[SP_MONITORS_AMOUNT] *
			   sizeof(struct hl_mon_state_dump));
	if (!monitors)
		return NULL;

	base_addr = sds->props[SP_NEXT_SYNC_OBJ_ADDR] * index;

	for (i = 0; i < sds->props[SP_MONITORS_AMOUNT]; ++i) {
		monitors[i].id = i;
		monitors[i].wr_addr_low =
			RREG32(base_addr + sds->props[SP_MON_OBJ_WR_ADDR_LOW] +
				i * sizeof(u32));

		monitors[i].wr_addr_high =
			RREG32(base_addr + sds->props[SP_MON_OBJ_WR_ADDR_HIGH] +
				i * sizeof(u32));

		monitors[i].wr_data =
			RREG32(base_addr + sds->props[SP_MON_OBJ_WR_DATA] +
				i * sizeof(u32));

		monitors[i].arm_data =
			RREG32(base_addr + sds->props[SP_MON_OBJ_ARM_DATA] +
				i * sizeof(u32));

		monitors[i].status =
			RREG32(base_addr + sds->props[SP_MON_OBJ_STATUS] +
				i * sizeof(u32));
	}

	return monitors;
}

 
static void hl_state_dump_free_monitors(struct hl_mon_state_dump *monitors)
{
	vfree(monitors);
}

 
static int hl_state_dump_print_monitors_single_block(struct hl_device *hdev,
						u32 index,
						char **buf, size_t *size,
						size_t *offset)
{
	struct hl_state_dump_specs *sds = &hdev->state_dump_specs;
	struct hl_mon_state_dump *monitors = NULL;
	int rc = 0, i;

	if (sds->sync_namager_names) {
		rc = hl_snprintf_resize(
			buf, size, offset, "%s\n",
			sds->sync_namager_names[index]);
		if (rc)
			goto out;
	}

	monitors = hl_state_dump_alloc_read_sm_block_monitors(hdev, index);
	if (!monitors) {
		rc = -ENOMEM;
		goto out;
	}

	for (i = 0; i < sds->props[SP_MONITORS_AMOUNT]; ++i) {
		if (!(sds->funcs.monitor_valid(&monitors[i])))
			continue;

		 
		rc = sds->funcs.print_single_monitor(buf, size, offset, hdev,
							&monitors[i]);
		if (rc)
			goto free_monitors;

		hl_snprintf_resize(buf, size, offset, "\n");
	}

free_monitors:
	hl_state_dump_free_monitors(monitors);
out:
	return rc;
}

 
static int hl_state_dump_print_monitors(struct hl_device *hdev,
					char **buf, size_t *size,
					size_t *offset)
{
	struct hl_state_dump_specs *sds = &hdev->state_dump_specs;
	u32 index;
	int rc = 0;

	rc = hl_snprintf_resize(buf, size, offset,
		"Valid (armed) monitor objects:\n");
	if (rc)
		goto out;

	if (sds->sync_namager_names) {
		for (index = 0; sds->sync_namager_names[index]; ++index) {
			rc = hl_state_dump_print_monitors_single_block(
				hdev, index, buf, size, offset);
			if (rc)
				goto out;
		}
	} else {
		for (index = 0; index < sds->props[SP_NUM_CORES]; ++index) {
			rc = hl_state_dump_print_monitors_single_block(
				hdev, index, buf, size, offset);
			if (rc)
				goto out;
		}
	}

out:
	return rc;
}

 
static int
hl_state_dump_print_engine_fences(struct hl_device *hdev,
				  enum hl_sync_engine_type engine_type,
				  char **buf, size_t *size, size_t *offset)
{
	struct hl_state_dump_specs *sds = &hdev->state_dump_specs;
	int rc = 0, i, n_fences;
	u64 base_addr, next_fence;

	switch (engine_type) {
	case ENGINE_TPC:
		n_fences = sds->props[SP_NUM_OF_TPC_ENGINES];
		base_addr = sds->props[SP_TPC0_CMDQ];
		next_fence = sds->props[SP_NEXT_TPC];
		break;
	case ENGINE_MME:
		n_fences = sds->props[SP_NUM_OF_MME_ENGINES];
		base_addr = sds->props[SP_MME_CMDQ];
		next_fence = sds->props[SP_NEXT_MME];
		break;
	case ENGINE_DMA:
		n_fences = sds->props[SP_NUM_OF_DMA_ENGINES];
		base_addr = sds->props[SP_DMA_CMDQ];
		next_fence = sds->props[SP_DMA_QUEUES_OFFSET];
		break;
	default:
		return -EINVAL;
	}
	for (i = 0; i < n_fences; ++i) {
		rc = sds->funcs.print_fences_single_engine(
			hdev,
			base_addr + next_fence * i +
				sds->props[SP_FENCE0_CNT_OFFSET],
			base_addr + next_fence * i +
				sds->props[SP_CP_STS_OFFSET],
			engine_type, i, buf, size, offset);
		if (rc)
			goto out;
	}
out:
	return rc;
}

 
static int hl_state_dump_print_fences(struct hl_device *hdev, char **buf,
				      size_t *size, size_t *offset)
{
	int rc = 0;

	rc = hl_snprintf_resize(buf, size, offset, "Valid (armed) fences:\n");
	if (rc)
		goto out;

	rc = hl_state_dump_print_engine_fences(hdev, ENGINE_TPC, buf, size, offset);
	if (rc)
		goto out;

	rc = hl_state_dump_print_engine_fences(hdev, ENGINE_MME, buf, size, offset);
	if (rc)
		goto out;

	rc = hl_state_dump_print_engine_fences(hdev, ENGINE_DMA, buf, size, offset);
	if (rc)
		goto out;

out:
	return rc;
}

 
int hl_state_dump(struct hl_device *hdev)
{
	char *buf = NULL;
	size_t offset = 0, size = 0;
	int rc;

	rc = hl_snprintf_resize(&buf, &size, &offset,
				"Timestamp taken on: %llu\n\n",
				ktime_to_ns(ktime_get()));
	if (rc)
		goto err;

	rc = hl_state_dump_print_syncs(hdev, &buf, &size, &offset);
	if (rc)
		goto err;

	hl_snprintf_resize(&buf, &size, &offset, "\n");

	rc = hl_state_dump_print_monitors(hdev, &buf, &size, &offset);
	if (rc)
		goto err;

	hl_snprintf_resize(&buf, &size, &offset, "\n");

	rc = hl_state_dump_print_fences(hdev, &buf, &size, &offset);
	if (rc)
		goto err;

	hl_snprintf_resize(&buf, &size, &offset, "\n");

	hl_debugfs_set_state_dump(hdev, buf, size);

	return 0;
err:
	vfree(buf);
	return rc;
}
