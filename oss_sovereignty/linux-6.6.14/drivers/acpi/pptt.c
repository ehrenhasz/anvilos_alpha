
 
#define pr_fmt(fmt) "ACPI PPTT: " fmt

#include <linux/acpi.h>
#include <linux/cacheinfo.h>
#include <acpi/processor.h>

static struct acpi_subtable_header *fetch_pptt_subtable(struct acpi_table_header *table_hdr,
							u32 pptt_ref)
{
	struct acpi_subtable_header *entry;

	 
	if (pptt_ref < sizeof(struct acpi_subtable_header))
		return NULL;

	if (pptt_ref + sizeof(struct acpi_subtable_header) > table_hdr->length)
		return NULL;

	entry = ACPI_ADD_PTR(struct acpi_subtable_header, table_hdr, pptt_ref);

	if (entry->length == 0)
		return NULL;

	if (pptt_ref + entry->length > table_hdr->length)
		return NULL;

	return entry;
}

static struct acpi_pptt_processor *fetch_pptt_node(struct acpi_table_header *table_hdr,
						   u32 pptt_ref)
{
	return (struct acpi_pptt_processor *)fetch_pptt_subtable(table_hdr, pptt_ref);
}

static struct acpi_pptt_cache *fetch_pptt_cache(struct acpi_table_header *table_hdr,
						u32 pptt_ref)
{
	return (struct acpi_pptt_cache *)fetch_pptt_subtable(table_hdr, pptt_ref);
}

static struct acpi_subtable_header *acpi_get_pptt_resource(struct acpi_table_header *table_hdr,
							   struct acpi_pptt_processor *node,
							   int resource)
{
	u32 *ref;

	if (resource >= node->number_of_priv_resources)
		return NULL;

	ref = ACPI_ADD_PTR(u32, node, sizeof(struct acpi_pptt_processor));
	ref += resource;

	return fetch_pptt_subtable(table_hdr, *ref);
}

static inline bool acpi_pptt_match_type(int table_type, int type)
{
	return ((table_type & ACPI_PPTT_MASK_CACHE_TYPE) == type ||
		table_type & ACPI_PPTT_CACHE_TYPE_UNIFIED & type);
}

 
static unsigned int acpi_pptt_walk_cache(struct acpi_table_header *table_hdr,
					 unsigned int local_level,
					 unsigned int *split_levels,
					 struct acpi_subtable_header *res,
					 struct acpi_pptt_cache **found,
					 unsigned int level, int type)
{
	struct acpi_pptt_cache *cache;

	if (res->type != ACPI_PPTT_TYPE_CACHE)
		return 0;

	cache = (struct acpi_pptt_cache *) res;
	while (cache) {
		local_level++;

		if (!(cache->flags & ACPI_PPTT_CACHE_TYPE_VALID)) {
			cache = fetch_pptt_cache(table_hdr, cache->next_level_of_cache);
			continue;
		}

		if (split_levels &&
		    (acpi_pptt_match_type(cache->attributes, ACPI_PPTT_CACHE_TYPE_DATA) ||
		     acpi_pptt_match_type(cache->attributes, ACPI_PPTT_CACHE_TYPE_INSTR)))
			*split_levels = local_level;

		if (local_level == level &&
		    acpi_pptt_match_type(cache->attributes, type)) {
			if (*found != NULL && cache != *found)
				pr_warn("Found duplicate cache level/type unable to determine uniqueness\n");

			pr_debug("Found cache @ level %u\n", level);
			*found = cache;
			 
		}
		cache = fetch_pptt_cache(table_hdr, cache->next_level_of_cache);
	}
	return local_level;
}

static struct acpi_pptt_cache *
acpi_find_cache_level(struct acpi_table_header *table_hdr,
		      struct acpi_pptt_processor *cpu_node,
		      unsigned int *starting_level, unsigned int *split_levels,
		      unsigned int level, int type)
{
	struct acpi_subtable_header *res;
	unsigned int number_of_levels = *starting_level;
	int resource = 0;
	struct acpi_pptt_cache *ret = NULL;
	unsigned int local_level;

	 
	while ((res = acpi_get_pptt_resource(table_hdr, cpu_node, resource))) {
		resource++;

		local_level = acpi_pptt_walk_cache(table_hdr, *starting_level,
						   split_levels, res, &ret,
						   level, type);
		 
		if (number_of_levels < local_level)
			number_of_levels = local_level;
	}
	if (number_of_levels > *starting_level)
		*starting_level = number_of_levels;

	return ret;
}

 
static void acpi_count_levels(struct acpi_table_header *table_hdr,
			      struct acpi_pptt_processor *cpu_node,
			      unsigned int *levels, unsigned int *split_levels)
{
	do {
		acpi_find_cache_level(table_hdr, cpu_node, levels, split_levels, 0, 0);
		cpu_node = fetch_pptt_node(table_hdr, cpu_node->parent);
	} while (cpu_node);
}

 
static int acpi_pptt_leaf_node(struct acpi_table_header *table_hdr,
			       struct acpi_pptt_processor *node)
{
	struct acpi_subtable_header *entry;
	unsigned long table_end;
	u32 node_entry;
	struct acpi_pptt_processor *cpu_node;
	u32 proc_sz;

	if (table_hdr->revision > 1)
		return (node->flags & ACPI_PPTT_ACPI_LEAF_NODE);

	table_end = (unsigned long)table_hdr + table_hdr->length;
	node_entry = ACPI_PTR_DIFF(node, table_hdr);
	entry = ACPI_ADD_PTR(struct acpi_subtable_header, table_hdr,
			     sizeof(struct acpi_table_pptt));
	proc_sz = sizeof(struct acpi_pptt_processor *);

	while ((unsigned long)entry + proc_sz < table_end) {
		cpu_node = (struct acpi_pptt_processor *)entry;
		if (entry->type == ACPI_PPTT_TYPE_PROCESSOR &&
		    cpu_node->parent == node_entry)
			return 0;
		if (entry->length == 0)
			return 0;
		entry = ACPI_ADD_PTR(struct acpi_subtable_header, entry,
				     entry->length);

	}
	return 1;
}

 
static struct acpi_pptt_processor *acpi_find_processor_node(struct acpi_table_header *table_hdr,
							    u32 acpi_cpu_id)
{
	struct acpi_subtable_header *entry;
	unsigned long table_end;
	struct acpi_pptt_processor *cpu_node;
	u32 proc_sz;

	table_end = (unsigned long)table_hdr + table_hdr->length;
	entry = ACPI_ADD_PTR(struct acpi_subtable_header, table_hdr,
			     sizeof(struct acpi_table_pptt));
	proc_sz = sizeof(struct acpi_pptt_processor *);

	 
	while ((unsigned long)entry + proc_sz < table_end) {
		cpu_node = (struct acpi_pptt_processor *)entry;

		if (entry->length == 0) {
			pr_warn("Invalid zero length subtable\n");
			break;
		}
		if (entry->type == ACPI_PPTT_TYPE_PROCESSOR &&
		    acpi_cpu_id == cpu_node->acpi_processor_id &&
		     acpi_pptt_leaf_node(table_hdr, cpu_node)) {
			return (struct acpi_pptt_processor *)entry;
		}

		entry = ACPI_ADD_PTR(struct acpi_subtable_header, entry,
				     entry->length);
	}

	return NULL;
}

static u8 acpi_cache_type(enum cache_type type)
{
	switch (type) {
	case CACHE_TYPE_DATA:
		pr_debug("Looking for data cache\n");
		return ACPI_PPTT_CACHE_TYPE_DATA;
	case CACHE_TYPE_INST:
		pr_debug("Looking for instruction cache\n");
		return ACPI_PPTT_CACHE_TYPE_INSTR;
	default:
	case CACHE_TYPE_UNIFIED:
		pr_debug("Looking for unified cache\n");
		 
		return ACPI_PPTT_CACHE_TYPE_UNIFIED;
	}
}

static struct acpi_pptt_cache *acpi_find_cache_node(struct acpi_table_header *table_hdr,
						    u32 acpi_cpu_id,
						    enum cache_type type,
						    unsigned int level,
						    struct acpi_pptt_processor **node)
{
	unsigned int total_levels = 0;
	struct acpi_pptt_cache *found = NULL;
	struct acpi_pptt_processor *cpu_node;
	u8 acpi_type = acpi_cache_type(type);

	pr_debug("Looking for CPU %d's level %u cache type %d\n",
		 acpi_cpu_id, level, acpi_type);

	cpu_node = acpi_find_processor_node(table_hdr, acpi_cpu_id);

	while (cpu_node && !found) {
		found = acpi_find_cache_level(table_hdr, cpu_node,
					      &total_levels, NULL, level, acpi_type);
		*node = cpu_node;
		cpu_node = fetch_pptt_node(table_hdr, cpu_node->parent);
	}

	return found;
}

 
static void update_cache_properties(struct cacheinfo *this_leaf,
				    struct acpi_pptt_cache *found_cache,
				    struct acpi_pptt_processor *cpu_node,
				    u8 revision)
{
	struct acpi_pptt_cache_v1* found_cache_v1;

	this_leaf->fw_token = cpu_node;
	if (found_cache->flags & ACPI_PPTT_SIZE_PROPERTY_VALID)
		this_leaf->size = found_cache->size;
	if (found_cache->flags & ACPI_PPTT_LINE_SIZE_VALID)
		this_leaf->coherency_line_size = found_cache->line_size;
	if (found_cache->flags & ACPI_PPTT_NUMBER_OF_SETS_VALID)
		this_leaf->number_of_sets = found_cache->number_of_sets;
	if (found_cache->flags & ACPI_PPTT_ASSOCIATIVITY_VALID)
		this_leaf->ways_of_associativity = found_cache->associativity;
	if (found_cache->flags & ACPI_PPTT_WRITE_POLICY_VALID) {
		switch (found_cache->attributes & ACPI_PPTT_MASK_WRITE_POLICY) {
		case ACPI_PPTT_CACHE_POLICY_WT:
			this_leaf->attributes = CACHE_WRITE_THROUGH;
			break;
		case ACPI_PPTT_CACHE_POLICY_WB:
			this_leaf->attributes = CACHE_WRITE_BACK;
			break;
		}
	}
	if (found_cache->flags & ACPI_PPTT_ALLOCATION_TYPE_VALID) {
		switch (found_cache->attributes & ACPI_PPTT_MASK_ALLOCATION_TYPE) {
		case ACPI_PPTT_CACHE_READ_ALLOCATE:
			this_leaf->attributes |= CACHE_READ_ALLOCATE;
			break;
		case ACPI_PPTT_CACHE_WRITE_ALLOCATE:
			this_leaf->attributes |= CACHE_WRITE_ALLOCATE;
			break;
		case ACPI_PPTT_CACHE_RW_ALLOCATE:
		case ACPI_PPTT_CACHE_RW_ALLOCATE_ALT:
			this_leaf->attributes |=
				CACHE_READ_ALLOCATE | CACHE_WRITE_ALLOCATE;
			break;
		}
	}
	 
	if (this_leaf->type == CACHE_TYPE_NOCACHE &&
	    found_cache->flags & ACPI_PPTT_CACHE_TYPE_VALID)
		this_leaf->type = CACHE_TYPE_UNIFIED;

	if (revision >= 3 && (found_cache->flags & ACPI_PPTT_CACHE_ID_VALID)) {
		found_cache_v1 = ACPI_ADD_PTR(struct acpi_pptt_cache_v1,
	                                      found_cache, sizeof(struct acpi_pptt_cache));
		this_leaf->id = found_cache_v1->cache_id;
		this_leaf->attributes |= CACHE_ID;
	}
}

static void cache_setup_acpi_cpu(struct acpi_table_header *table,
				 unsigned int cpu)
{
	struct acpi_pptt_cache *found_cache;
	struct cpu_cacheinfo *this_cpu_ci = get_cpu_cacheinfo(cpu);
	u32 acpi_cpu_id = get_acpi_id_for_cpu(cpu);
	struct cacheinfo *this_leaf;
	unsigned int index = 0;
	struct acpi_pptt_processor *cpu_node = NULL;

	while (index < get_cpu_cacheinfo(cpu)->num_leaves) {
		this_leaf = this_cpu_ci->info_list + index;
		found_cache = acpi_find_cache_node(table, acpi_cpu_id,
						   this_leaf->type,
						   this_leaf->level,
						   &cpu_node);
		pr_debug("found = %p %p\n", found_cache, cpu_node);
		if (found_cache)
			update_cache_properties(this_leaf, found_cache,
						ACPI_TO_POINTER(ACPI_PTR_DIFF(cpu_node, table)),
						table->revision);

		index++;
	}
}

static bool flag_identical(struct acpi_table_header *table_hdr,
			   struct acpi_pptt_processor *cpu)
{
	struct acpi_pptt_processor *next;

	 
	if (table_hdr->revision < 2)
		return false;

	 
	if (cpu->flags & ACPI_PPTT_ACPI_IDENTICAL) {
		next = fetch_pptt_node(table_hdr, cpu->parent);
		if (!(next && next->flags & ACPI_PPTT_ACPI_IDENTICAL))
			return true;
	}

	return false;
}

 
#define PPTT_ABORT_PACKAGE 0xFF

static struct acpi_pptt_processor *acpi_find_processor_tag(struct acpi_table_header *table_hdr,
							   struct acpi_pptt_processor *cpu,
							   int level, int flag)
{
	struct acpi_pptt_processor *prev_node;

	while (cpu && level) {
		 
		if (flag == ACPI_PPTT_ACPI_IDENTICAL) {
			if (flag_identical(table_hdr, cpu))
				break;
		} else if (cpu->flags & flag)
			break;
		pr_debug("level %d\n", level);
		prev_node = fetch_pptt_node(table_hdr, cpu->parent);
		if (prev_node == NULL)
			break;
		cpu = prev_node;
		level--;
	}
	return cpu;
}

static void acpi_pptt_warn_missing(void)
{
	pr_warn_once("No PPTT table found, CPU and cache topology may be inaccurate\n");
}

 
static int topology_get_acpi_cpu_tag(struct acpi_table_header *table,
				     unsigned int cpu, int level, int flag)
{
	struct acpi_pptt_processor *cpu_node;
	u32 acpi_cpu_id = get_acpi_id_for_cpu(cpu);

	cpu_node = acpi_find_processor_node(table, acpi_cpu_id);
	if (cpu_node) {
		cpu_node = acpi_find_processor_tag(table, cpu_node,
						   level, flag);
		 
		if (level == 0 ||
		    cpu_node->flags & ACPI_PPTT_ACPI_PROCESSOR_ID_VALID)
			return cpu_node->acpi_processor_id;
		return ACPI_PTR_DIFF(cpu_node, table);
	}
	pr_warn_once("PPTT table found, but unable to locate core %d (%d)\n",
		    cpu, acpi_cpu_id);
	return -ENOENT;
}


static struct acpi_table_header *acpi_get_pptt(void)
{
	static struct acpi_table_header *pptt;
	static bool is_pptt_checked;
	acpi_status status;

	 
	if (!pptt && !is_pptt_checked) {
		status = acpi_get_table(ACPI_SIG_PPTT, 0, &pptt);
		if (ACPI_FAILURE(status))
			acpi_pptt_warn_missing();

		is_pptt_checked = true;
	}

	return pptt;
}

static int find_acpi_cpu_topology_tag(unsigned int cpu, int level, int flag)
{
	struct acpi_table_header *table;
	int retval;

	table = acpi_get_pptt();
	if (!table)
		return -ENOENT;

	retval = topology_get_acpi_cpu_tag(table, cpu, level, flag);
	pr_debug("Topology Setup ACPI CPU %d, level %d ret = %d\n",
		 cpu, level, retval);

	return retval;
}

 
static int check_acpi_cpu_flag(unsigned int cpu, int rev, u32 flag)
{
	struct acpi_table_header *table;
	u32 acpi_cpu_id = get_acpi_id_for_cpu(cpu);
	struct acpi_pptt_processor *cpu_node = NULL;
	int ret = -ENOENT;

	table = acpi_get_pptt();
	if (!table)
		return -ENOENT;

	if (table->revision >= rev)
		cpu_node = acpi_find_processor_node(table, acpi_cpu_id);

	if (cpu_node)
		ret = (cpu_node->flags & flag) != 0;

	return ret;
}

 
int acpi_get_cache_info(unsigned int cpu, unsigned int *levels,
			unsigned int *split_levels)
{
	struct acpi_pptt_processor *cpu_node;
	struct acpi_table_header *table;
	u32 acpi_cpu_id;

	*levels = 0;
	if (split_levels)
		*split_levels = 0;

	table = acpi_get_pptt();
	if (!table)
		return -ENOENT;

	pr_debug("Cache Setup: find cache levels for CPU=%d\n", cpu);

	acpi_cpu_id = get_acpi_id_for_cpu(cpu);
	cpu_node = acpi_find_processor_node(table, acpi_cpu_id);
	if (!cpu_node)
		return -ENOENT;

	acpi_count_levels(table, cpu_node, levels, split_levels);

	pr_debug("Cache Setup: last_level=%d split_levels=%d\n",
		 *levels, split_levels ? *split_levels : -1);

	return 0;
}

 
int cache_setup_acpi(unsigned int cpu)
{
	struct acpi_table_header *table;

	table = acpi_get_pptt();
	if (!table)
		return -ENOENT;

	pr_debug("Cache Setup ACPI CPU %d\n", cpu);

	cache_setup_acpi_cpu(table, cpu);

	return 0;
}

 
int acpi_pptt_cpu_is_thread(unsigned int cpu)
{
	return check_acpi_cpu_flag(cpu, 2, ACPI_PPTT_ACPI_PROCESSOR_IS_THREAD);
}

 
int find_acpi_cpu_topology(unsigned int cpu, int level)
{
	return find_acpi_cpu_topology_tag(cpu, level, 0);
}

 
int find_acpi_cpu_topology_package(unsigned int cpu)
{
	return find_acpi_cpu_topology_tag(cpu, PPTT_ABORT_PACKAGE,
					  ACPI_PPTT_PHYSICAL_PACKAGE);
}

 

int find_acpi_cpu_topology_cluster(unsigned int cpu)
{
	struct acpi_table_header *table;
	struct acpi_pptt_processor *cpu_node, *cluster_node;
	u32 acpi_cpu_id;
	int retval;
	int is_thread;

	table = acpi_get_pptt();
	if (!table)
		return -ENOENT;

	acpi_cpu_id = get_acpi_id_for_cpu(cpu);
	cpu_node = acpi_find_processor_node(table, acpi_cpu_id);
	if (!cpu_node || !cpu_node->parent)
		return -ENOENT;

	is_thread = cpu_node->flags & ACPI_PPTT_ACPI_PROCESSOR_IS_THREAD;
	cluster_node = fetch_pptt_node(table, cpu_node->parent);
	if (!cluster_node)
		return -ENOENT;

	if (is_thread) {
		if (!cluster_node->parent)
			return -ENOENT;

		cluster_node = fetch_pptt_node(table, cluster_node->parent);
		if (!cluster_node)
			return -ENOENT;
	}
	if (cluster_node->flags & ACPI_PPTT_ACPI_PROCESSOR_ID_VALID)
		retval = cluster_node->acpi_processor_id;
	else
		retval = ACPI_PTR_DIFF(cluster_node, table);

	return retval;
}

 
int find_acpi_cpu_topology_hetero_id(unsigned int cpu)
{
	return find_acpi_cpu_topology_tag(cpu, PPTT_ABORT_PACKAGE,
					  ACPI_PPTT_ACPI_IDENTICAL);
}
