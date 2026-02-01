
 
#include "libfdt_env.h"

#include <fdt.h>
#include <libfdt.h>

#include "libfdt_internal.h"

 
static uint32_t overlay_get_target_phandle(const void *fdto, int fragment)
{
	const fdt32_t *val;
	int len;

	val = fdt_getprop(fdto, fragment, "target", &len);
	if (!val)
		return 0;

	if ((len != sizeof(*val)) || (fdt32_to_cpu(*val) == (uint32_t)-1))
		return (uint32_t)-1;

	return fdt32_to_cpu(*val);
}

int fdt_overlay_target_offset(const void *fdt, const void *fdto,
			      int fragment_offset, char const **pathp)
{
	uint32_t phandle;
	const char *path = NULL;
	int path_len = 0, ret;

	 
	phandle = overlay_get_target_phandle(fdto, fragment_offset);
	if (phandle == (uint32_t)-1)
		return -FDT_ERR_BADPHANDLE;

	 
	if (!phandle) {
		 
		path = fdt_getprop(fdto, fragment_offset, "target-path", &path_len);
		if (path)
			ret = fdt_path_offset(fdt, path);
		else
			ret = path_len;
	} else
		ret = fdt_node_offset_by_phandle(fdt, phandle);

	 
	if (ret < 0 && path_len == -FDT_ERR_NOTFOUND)
		ret = -FDT_ERR_BADOVERLAY;

	 
	if (ret < 0)
		return ret;

	 
	if (pathp)
		*pathp = path ? path : NULL;

	return ret;
}

 
static int overlay_phandle_add_offset(void *fdt, int node,
				      const char *name, uint32_t delta)
{
	const fdt32_t *val;
	uint32_t adj_val;
	int len;

	val = fdt_getprop(fdt, node, name, &len);
	if (!val)
		return len;

	if (len != sizeof(*val))
		return -FDT_ERR_BADPHANDLE;

	adj_val = fdt32_to_cpu(*val);
	if ((adj_val + delta) < adj_val)
		return -FDT_ERR_NOPHANDLES;

	adj_val += delta;
	if (adj_val == (uint32_t)-1)
		return -FDT_ERR_NOPHANDLES;

	return fdt_setprop_inplace_u32(fdt, node, name, adj_val);
}

 
static int overlay_adjust_node_phandles(void *fdto, int node,
					uint32_t delta)
{
	int child;
	int ret;

	ret = overlay_phandle_add_offset(fdto, node, "phandle", delta);
	if (ret && ret != -FDT_ERR_NOTFOUND)
		return ret;

	ret = overlay_phandle_add_offset(fdto, node, "linux,phandle", delta);
	if (ret && ret != -FDT_ERR_NOTFOUND)
		return ret;

	fdt_for_each_subnode(child, fdto, node) {
		ret = overlay_adjust_node_phandles(fdto, child, delta);
		if (ret)
			return ret;
	}

	return 0;
}

 
static int overlay_adjust_local_phandles(void *fdto, uint32_t delta)
{
	 
	return overlay_adjust_node_phandles(fdto, 0, delta);
}

 
static int overlay_update_local_node_references(void *fdto,
						int tree_node,
						int fixup_node,
						uint32_t delta)
{
	int fixup_prop;
	int fixup_child;
	int ret;

	fdt_for_each_property_offset(fixup_prop, fdto, fixup_node) {
		const fdt32_t *fixup_val;
		const char *tree_val;
		const char *name;
		int fixup_len;
		int tree_len;
		int i;

		fixup_val = fdt_getprop_by_offset(fdto, fixup_prop,
						  &name, &fixup_len);
		if (!fixup_val)
			return fixup_len;

		if (fixup_len % sizeof(uint32_t))
			return -FDT_ERR_BADOVERLAY;
		fixup_len /= sizeof(uint32_t);

		tree_val = fdt_getprop(fdto, tree_node, name, &tree_len);
		if (!tree_val) {
			if (tree_len == -FDT_ERR_NOTFOUND)
				return -FDT_ERR_BADOVERLAY;

			return tree_len;
		}

		for (i = 0; i < fixup_len; i++) {
			fdt32_t adj_val;
			uint32_t poffset;

			poffset = fdt32_to_cpu(fixup_val[i]);

			 
			memcpy(&adj_val, tree_val + poffset, sizeof(adj_val));

			adj_val = cpu_to_fdt32(fdt32_to_cpu(adj_val) + delta);

			ret = fdt_setprop_inplace_namelen_partial(fdto,
								  tree_node,
								  name,
								  strlen(name),
								  poffset,
								  &adj_val,
								  sizeof(adj_val));
			if (ret == -FDT_ERR_NOSPACE)
				return -FDT_ERR_BADOVERLAY;

			if (ret)
				return ret;
		}
	}

	fdt_for_each_subnode(fixup_child, fdto, fixup_node) {
		const char *fixup_child_name = fdt_get_name(fdto, fixup_child,
							    NULL);
		int tree_child;

		tree_child = fdt_subnode_offset(fdto, tree_node,
						fixup_child_name);
		if (tree_child == -FDT_ERR_NOTFOUND)
			return -FDT_ERR_BADOVERLAY;
		if (tree_child < 0)
			return tree_child;

		ret = overlay_update_local_node_references(fdto,
							   tree_child,
							   fixup_child,
							   delta);
		if (ret)
			return ret;
	}

	return 0;
}

 
static int overlay_update_local_references(void *fdto, uint32_t delta)
{
	int fixups;

	fixups = fdt_path_offset(fdto, "/__local_fixups__");
	if (fixups < 0) {
		 
		if (fixups == -FDT_ERR_NOTFOUND)
			return 0;

		return fixups;
	}

	 
	return overlay_update_local_node_references(fdto, 0, fixups,
						    delta);
}

 
static int overlay_fixup_one_phandle(void *fdt, void *fdto,
				     int symbols_off,
				     const char *path, uint32_t path_len,
				     const char *name, uint32_t name_len,
				     int poffset, const char *label)
{
	const char *symbol_path;
	uint32_t phandle;
	fdt32_t phandle_prop;
	int symbol_off, fixup_off;
	int prop_len;

	if (symbols_off < 0)
		return symbols_off;

	symbol_path = fdt_getprop(fdt, symbols_off, label,
				  &prop_len);
	if (!symbol_path)
		return prop_len;

	symbol_off = fdt_path_offset(fdt, symbol_path);
	if (symbol_off < 0)
		return symbol_off;

	phandle = fdt_get_phandle(fdt, symbol_off);
	if (!phandle)
		return -FDT_ERR_NOTFOUND;

	fixup_off = fdt_path_offset_namelen(fdto, path, path_len);
	if (fixup_off == -FDT_ERR_NOTFOUND)
		return -FDT_ERR_BADOVERLAY;
	if (fixup_off < 0)
		return fixup_off;

	phandle_prop = cpu_to_fdt32(phandle);
	return fdt_setprop_inplace_namelen_partial(fdto, fixup_off,
						   name, name_len, poffset,
						   &phandle_prop,
						   sizeof(phandle_prop));
};

 
static int overlay_fixup_phandle(void *fdt, void *fdto, int symbols_off,
				 int property)
{
	const char *value;
	const char *label;
	int len;

	value = fdt_getprop_by_offset(fdto, property,
				      &label, &len);
	if (!value) {
		if (len == -FDT_ERR_NOTFOUND)
			return -FDT_ERR_INTERNAL;

		return len;
	}

	do {
		const char *path, *name, *fixup_end;
		const char *fixup_str = value;
		uint32_t path_len, name_len;
		uint32_t fixup_len;
		char *sep, *endptr;
		int poffset, ret;

		fixup_end = memchr(value, '\0', len);
		if (!fixup_end)
			return -FDT_ERR_BADOVERLAY;
		fixup_len = fixup_end - fixup_str;

		len -= fixup_len + 1;
		value += fixup_len + 1;

		path = fixup_str;
		sep = memchr(fixup_str, ':', fixup_len);
		if (!sep || *sep != ':')
			return -FDT_ERR_BADOVERLAY;

		path_len = sep - path;
		if (path_len == (fixup_len - 1))
			return -FDT_ERR_BADOVERLAY;

		fixup_len -= path_len + 1;
		name = sep + 1;
		sep = memchr(name, ':', fixup_len);
		if (!sep || *sep != ':')
			return -FDT_ERR_BADOVERLAY;

		name_len = sep - name;
		if (!name_len)
			return -FDT_ERR_BADOVERLAY;

		poffset = strtoul(sep + 1, &endptr, 10);
		if ((*endptr != '\0') || (endptr <= (sep + 1)))
			return -FDT_ERR_BADOVERLAY;

		ret = overlay_fixup_one_phandle(fdt, fdto, symbols_off,
						path, path_len, name, name_len,
						poffset, label);
		if (ret)
			return ret;
	} while (len > 0);

	return 0;
}

 
static int overlay_fixup_phandles(void *fdt, void *fdto)
{
	int fixups_off, symbols_off;
	int property;

	 
	fixups_off = fdt_path_offset(fdto, "/__fixups__");
	if (fixups_off == -FDT_ERR_NOTFOUND)
		return 0;  
	if (fixups_off < 0)
		return fixups_off;

	 
	symbols_off = fdt_path_offset(fdt, "/__symbols__");
	if ((symbols_off < 0 && (symbols_off != -FDT_ERR_NOTFOUND)))
		return symbols_off;

	fdt_for_each_property_offset(property, fdto, fixups_off) {
		int ret;

		ret = overlay_fixup_phandle(fdt, fdto, symbols_off, property);
		if (ret)
			return ret;
	}

	return 0;
}

 
static int overlay_apply_node(void *fdt, int target,
			      void *fdto, int node)
{
	int property;
	int subnode;

	fdt_for_each_property_offset(property, fdto, node) {
		const char *name;
		const void *prop;
		int prop_len;
		int ret;

		prop = fdt_getprop_by_offset(fdto, property, &name,
					     &prop_len);
		if (prop_len == -FDT_ERR_NOTFOUND)
			return -FDT_ERR_INTERNAL;
		if (prop_len < 0)
			return prop_len;

		ret = fdt_setprop(fdt, target, name, prop, prop_len);
		if (ret)
			return ret;
	}

	fdt_for_each_subnode(subnode, fdto, node) {
		const char *name = fdt_get_name(fdto, subnode, NULL);
		int nnode;
		int ret;

		nnode = fdt_add_subnode(fdt, target, name);
		if (nnode == -FDT_ERR_EXISTS) {
			nnode = fdt_subnode_offset(fdt, target, name);
			if (nnode == -FDT_ERR_NOTFOUND)
				return -FDT_ERR_INTERNAL;
		}

		if (nnode < 0)
			return nnode;

		ret = overlay_apply_node(fdt, nnode, fdto, subnode);
		if (ret)
			return ret;
	}

	return 0;
}

 
static int overlay_merge(void *fdt, void *fdto)
{
	int fragment;

	fdt_for_each_subnode(fragment, fdto, 0) {
		int overlay;
		int target;
		int ret;

		 
		overlay = fdt_subnode_offset(fdto, fragment, "__overlay__");
		if (overlay == -FDT_ERR_NOTFOUND)
			continue;

		if (overlay < 0)
			return overlay;

		target = fdt_overlay_target_offset(fdt, fdto, fragment, NULL);
		if (target < 0)
			return target;

		ret = overlay_apply_node(fdt, target, fdto, overlay);
		if (ret)
			return ret;
	}

	return 0;
}

static int get_path_len(const void *fdt, int nodeoffset)
{
	int len = 0, namelen;
	const char *name;

	FDT_RO_PROBE(fdt);

	for (;;) {
		name = fdt_get_name(fdt, nodeoffset, &namelen);
		if (!name)
			return namelen;

		 
		if (namelen == 0)
			break;

		nodeoffset = fdt_parent_offset(fdt, nodeoffset);
		if (nodeoffset < 0)
			return nodeoffset;
		len += namelen + 1;
	}

	 
	if (len == 0)
		len++;
	return len;
}

 
static int overlay_symbol_update(void *fdt, void *fdto)
{
	int root_sym, ov_sym, prop, path_len, fragment, target;
	int len, frag_name_len, ret, rel_path_len;
	const char *s, *e;
	const char *path;
	const char *name;
	const char *frag_name;
	const char *rel_path;
	const char *target_path;
	char *buf;
	void *p;

	ov_sym = fdt_subnode_offset(fdto, 0, "__symbols__");

	 
	if (ov_sym < 0)
		return 0;

	root_sym = fdt_subnode_offset(fdt, 0, "__symbols__");

	 
	if (root_sym == -FDT_ERR_NOTFOUND)
		root_sym = fdt_add_subnode(fdt, 0, "__symbols__");

	 
	if (root_sym < 0)
		return root_sym;

	 
	fdt_for_each_property_offset(prop, fdto, ov_sym) {
		path = fdt_getprop_by_offset(fdto, prop, &name, &path_len);
		if (!path)
			return path_len;

		 
		if (path_len < 1 || memchr(path, '\0', path_len) != &path[path_len - 1])
			return -FDT_ERR_BADVALUE;

		 
		e = path + path_len;

		if (*path != '/')
			return -FDT_ERR_BADVALUE;

		 
		s = strchr(path + 1, '/');
		if (!s) {
			 
			continue;
		}

		frag_name = path + 1;
		frag_name_len = s - path - 1;

		 
		len = sizeof("/__overlay__/") - 1;
		if ((e - s) > len && (memcmp(s, "/__overlay__/", len) == 0)) {
			 
			rel_path = s + len;
			rel_path_len = e - rel_path - 1;
		} else if ((e - s) == len
			   && (memcmp(s, "/__overlay__", len - 1) == 0)) {
			 
			rel_path = "";
			rel_path_len = 0;
		} else {
			 
			continue;
		}

		 
		ret = fdt_subnode_offset_namelen(fdto, 0, frag_name,
					       frag_name_len);
		 
		if (ret < 0)
			return -FDT_ERR_BADOVERLAY;
		fragment = ret;

		 
		ret = fdt_subnode_offset(fdto, fragment, "__overlay__");
		if (ret < 0)
			return -FDT_ERR_BADOVERLAY;

		 
		ret = fdt_overlay_target_offset(fdt, fdto, fragment, &target_path);
		if (ret < 0)
			return ret;
		target = ret;

		 
		if (!target_path) {
			ret = get_path_len(fdt, target);
			if (ret < 0)
				return ret;
			len = ret;
		} else {
			len = strlen(target_path);
		}

		ret = fdt_setprop_placeholder(fdt, root_sym, name,
				len + (len > 1) + rel_path_len + 1, &p);
		if (ret < 0)
			return ret;

		if (!target_path) {
			 
			ret = fdt_overlay_target_offset(fdt, fdto, fragment, &target_path);
			if (ret < 0)
				return ret;
			target = ret;
		}

		buf = p;
		if (len > 1) {  
			if (!target_path) {
				ret = fdt_get_path(fdt, target, buf, len + 1);
				if (ret < 0)
					return ret;
			} else
				memcpy(buf, target_path, len + 1);

		} else
			len--;

		buf[len] = '/';
		memcpy(buf + len + 1, rel_path, rel_path_len);
		buf[len + 1 + rel_path_len] = '\0';
	}

	return 0;
}

int fdt_overlay_apply(void *fdt, void *fdto)
{
	uint32_t delta;
	int ret;

	FDT_RO_PROBE(fdt);
	FDT_RO_PROBE(fdto);

	ret = fdt_find_max_phandle(fdt, &delta);
	if (ret)
		goto err;

	ret = overlay_adjust_local_phandles(fdto, delta);
	if (ret)
		goto err;

	ret = overlay_update_local_references(fdto, delta);
	if (ret)
		goto err;

	ret = overlay_fixup_phandles(fdt, fdto);
	if (ret)
		goto err;

	ret = overlay_merge(fdt, fdto);
	if (ret)
		goto err;

	ret = overlay_symbol_update(fdt, fdto);
	if (ret)
		goto err;

	 
	fdt_set_magic(fdto, ~0);

	return 0;

err:
	 
	fdt_set_magic(fdto, ~0);

	 
	fdt_set_magic(fdt, ~0);

	return ret;
}
