#include <sys/zfs_context.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/vdev_impl.h>
#include <sys/vdev_os.h>
#include <sys/fs/zfs.h>
#include <sys/zio.h>
#include <vm/vm_page.h>
#include <geom/geom.h>
#include <geom/geom_disk.h>
#include <geom/geom_int.h>
#ifndef g_topology_locked
#define	g_topology_locked()	sx_xlocked(&topology_lock)
#endif
static g_attrchanged_t vdev_geom_attrchanged;
struct g_class zfs_vdev_class = {
	.name = "ZFS::VDEV",
	.version = G_VERSION,
	.attrchanged = vdev_geom_attrchanged,
};
struct consumer_vdev_elem {
	SLIST_ENTRY(consumer_vdev_elem)	elems;
	vdev_t	*vd;
};
SLIST_HEAD(consumer_priv_t, consumer_vdev_elem);
_Static_assert(
    sizeof (((struct g_consumer *)NULL)->private) ==
    sizeof (struct consumer_priv_t *),
	"consumer_priv_t* can't be stored in g_consumer.private");
DECLARE_GEOM_CLASS(zfs_vdev_class, zfs_vdev);
SYSCTL_DECL(_vfs_zfs_vdev);
static int vdev_geom_bio_flush_disable;
SYSCTL_INT(_vfs_zfs_vdev, OID_AUTO, bio_flush_disable, CTLFLAG_RWTUN,
	&vdev_geom_bio_flush_disable, 0, "Disable BIO_FLUSH");
static int vdev_geom_bio_delete_disable;
SYSCTL_INT(_vfs_zfs_vdev, OID_AUTO, bio_delete_disable, CTLFLAG_RWTUN,
	&vdev_geom_bio_delete_disable, 0, "Disable BIO_DELETE");
static void vdev_geom_detach(struct g_consumer *cp, boolean_t open_for_read);
uint_t zfs_geom_probe_vdev_key;
static void
vdev_geom_set_physpath(vdev_t *vd, struct g_consumer *cp,
    boolean_t do_null_update)
{
	boolean_t needs_update = B_FALSE;
	char *physpath;
	int error, physpath_len;
	physpath_len = MAXPATHLEN;
	physpath = g_malloc(physpath_len, M_WAITOK|M_ZERO);
	error = g_io_getattr("GEOM::physpath", cp, &physpath_len, physpath);
	if (error == 0) {
		char *old_physpath;
		g_topology_assert();
		old_physpath = vd->vdev_physpath;
		vd->vdev_physpath = spa_strdup(physpath);
		if (old_physpath != NULL) {
			needs_update = (strcmp(old_physpath,
			    vd->vdev_physpath) != 0);
			spa_strfree(old_physpath);
		} else
			needs_update = do_null_update;
	}
	g_free(physpath);
	if (needs_update)
		spa_async_request(vd->vdev_spa, SPA_ASYNC_CONFIG_UPDATE);
}
static void
vdev_geom_attrchanged(struct g_consumer *cp, const char *attr)
{
	struct consumer_priv_t *priv;
	struct consumer_vdev_elem *elem;
	priv = (struct consumer_priv_t *)&cp->private;
	if (SLIST_EMPTY(priv))
		return;
	SLIST_FOREACH(elem, priv, elems) {
		vdev_t *vd = elem->vd;
		if (strcmp(attr, "GEOM::physpath") == 0) {
			vdev_geom_set_physpath(vd, cp,  B_TRUE);
			return;
		}
	}
}
static void
vdev_geom_resize(struct g_consumer *cp)
{
	struct consumer_priv_t *priv;
	struct consumer_vdev_elem *elem;
	spa_t *spa;
	vdev_t *vd;
	priv = (struct consumer_priv_t *)&cp->private;
	if (SLIST_EMPTY(priv))
		return;
	SLIST_FOREACH(elem, priv, elems) {
		vd = elem->vd;
		if (vd->vdev_state != VDEV_STATE_HEALTHY)
			continue;
		spa = vd->vdev_spa;
		if (!spa->spa_autoexpand)
			continue;
		vdev_online(spa, vd->vdev_guid, ZFS_ONLINE_EXPAND, NULL);
	}
}
static void
vdev_geom_orphan(struct g_consumer *cp)
{
	struct consumer_priv_t *priv;
	struct consumer_vdev_elem *elem;
	g_topology_assert();
	priv = (struct consumer_priv_t *)&cp->private;
	if (SLIST_EMPTY(priv))
		return;
	SLIST_FOREACH(elem, priv, elems) {
		vdev_t *vd = elem->vd;
		vd->vdev_remove_wanted = B_TRUE;
		spa_async_request(vd->vdev_spa, SPA_ASYNC_REMOVE);
	}
}
static struct g_consumer *
vdev_geom_attach(struct g_provider *pp, vdev_t *vd, boolean_t sanity)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error;
	g_topology_assert();
	ZFS_LOG(1, "Attaching to %s.", pp->name);
	if (sanity) {
		if (pp->sectorsize > VDEV_PAD_SIZE || !ISP2(pp->sectorsize)) {
			ZFS_LOG(1, "Failing attach of %s. "
			    "Incompatible sectorsize %d\n",
			    pp->name, pp->sectorsize);
			return (NULL);
		} else if (pp->mediasize < SPA_MINDEVSIZE) {
			ZFS_LOG(1, "Failing attach of %s. "
			    "Incompatible mediasize %ju\n",
			    pp->name, pp->mediasize);
			return (NULL);
		}
	}
	LIST_FOREACH(gp, &zfs_vdev_class.geom, geom) {
		if (gp->flags & G_GEOM_WITHER)
			continue;
		if (strcmp(gp->name, "zfs::vdev") != 0)
			continue;
		break;
	}
	if (gp == NULL) {
		gp = g_new_geomf(&zfs_vdev_class, "zfs::vdev");
		gp->orphan = vdev_geom_orphan;
		gp->attrchanged = vdev_geom_attrchanged;
		gp->resize = vdev_geom_resize;
		cp = g_new_consumer(gp);
		error = g_attach(cp, pp);
		if (error != 0) {
			ZFS_LOG(1, "%s(%d): g_attach failed: %d\n", __func__,
			    __LINE__, error);
			vdev_geom_detach(cp, B_FALSE);
			return (NULL);
		}
		error = g_access(cp, 1, 0, 1);
		if (error != 0) {
			ZFS_LOG(1, "%s(%d): g_access failed: %d\n", __func__,
			    __LINE__, error);
			vdev_geom_detach(cp, B_FALSE);
			return (NULL);
		}
		ZFS_LOG(1, "Created geom and consumer for %s.", pp->name);
	} else {
		LIST_FOREACH(cp, &gp->consumer, consumer) {
			if (cp->provider == pp) {
				ZFS_LOG(1, "Found consumer for %s.", pp->name);
				break;
			}
		}
		if (cp == NULL) {
			cp = g_new_consumer(gp);
			error = g_attach(cp, pp);
			if (error != 0) {
				ZFS_LOG(1, "%s(%d): g_attach failed: %d\n",
				    __func__, __LINE__, error);
				vdev_geom_detach(cp, B_FALSE);
				return (NULL);
			}
			error = g_access(cp, 1, 0, 1);
			if (error != 0) {
				ZFS_LOG(1, "%s(%d): g_access failed: %d\n",
				    __func__, __LINE__, error);
				vdev_geom_detach(cp, B_FALSE);
				return (NULL);
			}
			ZFS_LOG(1, "Created consumer for %s.", pp->name);
		} else {
			error = g_access(cp, 1, 0, 1);
			if (error != 0) {
				ZFS_LOG(1, "%s(%d): g_access failed: %d\n",
				    __func__, __LINE__, error);
				return (NULL);
			}
			ZFS_LOG(1, "Used existing consumer for %s.", pp->name);
		}
	}
	if (vd != NULL)
		vd->vdev_tsd = cp;
	cp->flags |= G_CF_DIRECT_SEND | G_CF_DIRECT_RECEIVE;
	return (cp);
}
static void
vdev_geom_detach(struct g_consumer *cp, boolean_t open_for_read)
{
	struct g_geom *gp;
	g_topology_assert();
	ZFS_LOG(1, "Detaching from %s.",
	    cp->provider && cp->provider->name ? cp->provider->name : "NULL");
	gp = cp->geom;
	if (open_for_read)
		g_access(cp, -1, 0, -1);
	if (cp->acr == 0 && cp->ace == 0) {
		if (cp->acw > 0)
			g_access(cp, 0, -cp->acw, 0);
		if (cp->provider != NULL) {
			ZFS_LOG(1, "Destroying consumer for %s.",
			    cp->provider->name ? cp->provider->name : "NULL");
			g_detach(cp);
		}
		g_destroy_consumer(cp);
	}
	if (LIST_EMPTY(&gp->consumer)) {
		ZFS_LOG(1, "Destroyed geom %s.", gp->name);
		g_wither_geom(gp, ENXIO);
	}
}
static void
vdev_geom_close_locked(vdev_t *vd)
{
	struct g_consumer *cp;
	struct consumer_priv_t *priv;
	struct consumer_vdev_elem *elem, *elem_temp;
	g_topology_assert();
	cp = vd->vdev_tsd;
	vd->vdev_delayed_close = B_FALSE;
	if (cp == NULL)
		return;
	ZFS_LOG(1, "Closing access to %s.", cp->provider->name);
	KASSERT(cp->private != NULL, ("%s: cp->private is NULL", __func__));
	priv = (struct consumer_priv_t *)&cp->private;
	vd->vdev_tsd = NULL;
	SLIST_FOREACH_SAFE(elem, priv, elems, elem_temp) {
		if (elem->vd == vd) {
			SLIST_REMOVE(priv, elem, consumer_vdev_elem, elems);
			g_free(elem);
		}
	}
	vdev_geom_detach(cp, B_TRUE);
}
static void
vdev_geom_io(struct g_consumer *cp, int *cmds, void **datas, off_t *offsets,
    off_t *sizes, int *errors, int ncmds)
{
	struct bio **bios;
	uint8_t *p;
	off_t off, maxio, s, end;
	int i, n_bios, j;
	size_t bios_size;
#if __FreeBSD_version > 1300130
	maxio = maxphys - (maxphys % cp->provider->sectorsize);
#else
	maxio = MAXPHYS - (MAXPHYS % cp->provider->sectorsize);
#endif
	n_bios = 0;
	for (i = 0; i < ncmds; i++)
		n_bios += (sizes[i] + maxio - 1) / maxio;
	bios_size = n_bios * sizeof (struct bio *);
	bios = kmem_zalloc(bios_size, KM_SLEEP);
	for (i = j = 0; i < ncmds; i++) {
		off = offsets[i];
		p = datas[i];
		s = sizes[i];
		end = off + s;
		ASSERT0(off % cp->provider->sectorsize);
		ASSERT0(s % cp->provider->sectorsize);
		for (; off < end; off += maxio, p += maxio, s -= maxio, j++) {
			bios[j] = g_alloc_bio();
			bios[j]->bio_cmd = cmds[i];
			bios[j]->bio_done = NULL;
			bios[j]->bio_offset = off;
			bios[j]->bio_length = MIN(s, maxio);
			bios[j]->bio_data = (caddr_t)p;
			g_io_request(bios[j], cp);
		}
	}
	ASSERT3S(j, ==, n_bios);
	for (i = j = 0; i < ncmds; i++) {
		off = offsets[i];
		s = sizes[i];
		end = off + s;
		for (; off < end; off += maxio, s -= maxio, j++) {
			errors[i] = biowait(bios[j], "vdev_geom_io") ||
			    errors[i];
			g_destroy_bio(bios[j]);
		}
	}
	kmem_free(bios, bios_size);
}
static int
vdev_geom_read_config(struct g_consumer *cp, nvlist_t **configp)
{
	struct g_provider *pp;
	nvlist_t *config;
	vdev_phys_t *vdev_lists[VDEV_LABELS];
	char *buf;
	size_t buflen;
	uint64_t psize, state, txg;
	off_t offsets[VDEV_LABELS];
	off_t size;
	off_t sizes[VDEV_LABELS];
	int cmds[VDEV_LABELS];
	int errors[VDEV_LABELS];
	int l, nlabels;
	g_topology_assert_not();
	pp = cp->provider;
	ZFS_LOG(1, "Reading config from %s...", pp->name);
	psize = pp->mediasize;
	psize = P2ALIGN(psize, (uint64_t)sizeof (vdev_label_t));
	size = sizeof (*vdev_lists[0]) + pp->sectorsize -
	    ((sizeof (*vdev_lists[0]) - 1) % pp->sectorsize) - 1;
	buflen = sizeof (vdev_lists[0]->vp_nvlist);
	for (l = 0; l < VDEV_LABELS; l++) {
		cmds[l] = BIO_READ;
		vdev_lists[l] = kmem_alloc(size, KM_SLEEP);
		offsets[l] = vdev_label_offset(psize, l, 0) + VDEV_SKIP_SIZE;
		sizes[l] = size;
		errors[l] = 0;
		ASSERT0(offsets[l] % pp->sectorsize);
	}
	vdev_geom_io(cp, cmds, (void**)vdev_lists, offsets, sizes, errors,
	    VDEV_LABELS);
	config = *configp = NULL;
	nlabels = 0;
	for (l = 0; l < VDEV_LABELS; l++) {
		if (errors[l] != 0)
			continue;
		buf = vdev_lists[l]->vp_nvlist;
		if (nvlist_unpack(buf, buflen, &config, 0) != 0)
			continue;
		if (nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_STATE,
		    &state) != 0 || state > POOL_STATE_L2CACHE) {
			nvlist_free(config);
			continue;
		}
		if (state != POOL_STATE_SPARE &&
		    state != POOL_STATE_L2CACHE &&
		    (nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_TXG,
		    &txg) != 0 || txg == 0)) {
			nvlist_free(config);
			continue;
		}
		if (*configp != NULL)
			nvlist_free(*configp);
		*configp = config;
		nlabels++;
	}
	for (l = 0; l < VDEV_LABELS; l++)
		kmem_free(vdev_lists[l], size);
	return (nlabels);
}
static void
resize_configs(nvlist_t ***configs, uint64_t *count, uint64_t id)
{
	nvlist_t **new_configs;
	uint64_t i;
	if (id < *count)
		return;
	new_configs = kmem_zalloc((id + 1) * sizeof (nvlist_t *),
	    KM_SLEEP);
	for (i = 0; i < *count; i++)
		new_configs[i] = (*configs)[i];
	if (*configs != NULL)
		kmem_free(*configs, *count * sizeof (void *));
	*configs = new_configs;
	*count = id + 1;
}
static void
process_vdev_config(nvlist_t ***configs, uint64_t *count, nvlist_t *cfg,
    const char *name, uint64_t *known_pool_guid)
{
	nvlist_t *vdev_tree;
	uint64_t pool_guid;
	uint64_t vdev_guid;
	uint64_t id, txg, known_txg;
	const char *pname;
	if (nvlist_lookup_string(cfg, ZPOOL_CONFIG_POOL_NAME, &pname) != 0 ||
	    strcmp(pname, name) != 0)
		goto ignore;
	if (nvlist_lookup_uint64(cfg, ZPOOL_CONFIG_POOL_GUID, &pool_guid) != 0)
		goto ignore;
	if (nvlist_lookup_uint64(cfg, ZPOOL_CONFIG_TOP_GUID, &vdev_guid) != 0)
		goto ignore;
	if (nvlist_lookup_nvlist(cfg, ZPOOL_CONFIG_VDEV_TREE, &vdev_tree) != 0)
		goto ignore;
	if (nvlist_lookup_uint64(vdev_tree, ZPOOL_CONFIG_ID, &id) != 0)
		goto ignore;
	txg = fnvlist_lookup_uint64(cfg, ZPOOL_CONFIG_POOL_TXG);
	if (*known_pool_guid != 0) {
		if (pool_guid != *known_pool_guid)
			goto ignore;
	} else
		*known_pool_guid = pool_guid;
	resize_configs(configs, count, id);
	if ((*configs)[id] != NULL) {
		known_txg = fnvlist_lookup_uint64((*configs)[id],
		    ZPOOL_CONFIG_POOL_TXG);
		if (txg <= known_txg)
			goto ignore;
		nvlist_free((*configs)[id]);
	}
	(*configs)[id] = cfg;
	return;
ignore:
	nvlist_free(cfg);
}
int
vdev_geom_read_pool_label(const char *name,
    nvlist_t ***configs, uint64_t *count)
{
	struct g_class *mp;
	struct g_geom *gp;
	struct g_provider *pp;
	struct g_consumer *zcp;
	nvlist_t *vdev_cfg;
	uint64_t pool_guid;
	int nlabels;
	DROP_GIANT();
	g_topology_lock();
	*configs = NULL;
	*count = 0;
	pool_guid = 0;
	LIST_FOREACH(mp, &g_classes, class) {
		if (mp == &zfs_vdev_class)
			continue;
		LIST_FOREACH(gp, &mp->geom, geom) {
			if (gp->flags & G_GEOM_WITHER)
				continue;
			LIST_FOREACH(pp, &gp->provider, provider) {
				if (pp->flags & G_PF_WITHER)
					continue;
				zcp = vdev_geom_attach(pp, NULL, B_TRUE);
				if (zcp == NULL)
					continue;
				g_topology_unlock();
				nlabels = vdev_geom_read_config(zcp, &vdev_cfg);
				g_topology_lock();
				vdev_geom_detach(zcp, B_TRUE);
				if (nlabels == 0)
					continue;
				ZFS_LOG(1, "successfully read vdev config");
				process_vdev_config(configs, count,
				    vdev_cfg, name, &pool_guid);
			}
		}
	}
	g_topology_unlock();
	PICKUP_GIANT();
	return (*count > 0 ? 0 : ENOENT);
}
enum match {
	NO_MATCH = 0,		 
	TOPGUID_MATCH = 1,	 
	ZERO_MATCH = 1,		 
	ONE_MATCH = 2,		 
	TWO_MATCH = 3,		 
	THREE_MATCH = 4,	 
	FULL_MATCH = 5		 
};
static enum match
vdev_attach_ok(vdev_t *vd, struct g_provider *pp)
{
	nvlist_t *config;
	uint64_t pool_guid, top_guid, vdev_guid;
	struct g_consumer *cp;
	int nlabels;
	cp = vdev_geom_attach(pp, NULL, B_TRUE);
	if (cp == NULL) {
		ZFS_LOG(1, "Unable to attach tasting instance to %s.",
		    pp->name);
		return (NO_MATCH);
	}
	g_topology_unlock();
	nlabels = vdev_geom_read_config(cp, &config);
	g_topology_lock();
	vdev_geom_detach(cp, B_TRUE);
	if (nlabels == 0) {
		ZFS_LOG(1, "Unable to read config from %s.", pp->name);
		return (NO_MATCH);
	}
	pool_guid = 0;
	(void) nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_GUID, &pool_guid);
	top_guid = 0;
	(void) nvlist_lookup_uint64(config, ZPOOL_CONFIG_TOP_GUID, &top_guid);
	vdev_guid = 0;
	(void) nvlist_lookup_uint64(config, ZPOOL_CONFIG_GUID, &vdev_guid);
	nvlist_free(config);
	if (pool_guid != 0 && pool_guid != spa_guid(vd->vdev_spa)) {
		ZFS_LOG(1, "pool guid mismatch for provider %s: %ju != %ju.",
		    pp->name,
		    (uintmax_t)spa_guid(vd->vdev_spa), (uintmax_t)pool_guid);
		return (NO_MATCH);
	}
	if (vdev_guid == vd->vdev_guid) {
		ZFS_LOG(1, "guids match for provider %s.", pp->name);
		return (ZERO_MATCH + nlabels);
	} else if (top_guid == vd->vdev_guid && vd == vd->vdev_top) {
		ZFS_LOG(1, "top vdev guid match for provider %s.", pp->name);
		return (TOPGUID_MATCH);
	}
	ZFS_LOG(1, "vdev guid mismatch for provider %s: %ju != %ju.",
	    pp->name, (uintmax_t)vd->vdev_guid, (uintmax_t)vdev_guid);
	return (NO_MATCH);
}
static struct g_consumer *
vdev_geom_attach_by_guids(vdev_t *vd)
{
	struct g_class *mp;
	struct g_geom *gp;
	struct g_provider *pp, *best_pp;
	struct g_consumer *cp;
	const char *vdpath;
	enum match match, best_match;
	g_topology_assert();
	vdpath = vd->vdev_path + sizeof ("/dev/") - 1;
	cp = NULL;
	best_pp = NULL;
	best_match = NO_MATCH;
	LIST_FOREACH(mp, &g_classes, class) {
		if (mp == &zfs_vdev_class)
			continue;
		LIST_FOREACH(gp, &mp->geom, geom) {
			if (gp->flags & G_GEOM_WITHER)
				continue;
			LIST_FOREACH(pp, &gp->provider, provider) {
				match = vdev_attach_ok(vd, pp);
				if (match > best_match) {
					best_match = match;
					best_pp = pp;
				} else if (match == best_match) {
					if (strcmp(pp->name, vdpath) == 0) {
						best_pp = pp;
					}
				}
				if (match == FULL_MATCH)
					goto out;
			}
		}
	}
out:
	if (best_pp) {
		cp = vdev_geom_attach(best_pp, vd, B_TRUE);
		if (cp == NULL) {
			printf("ZFS WARNING: Unable to attach to %s.\n",
			    best_pp->name);
		}
	}
	return (cp);
}
static struct g_consumer *
vdev_geom_open_by_guids(vdev_t *vd)
{
	struct g_consumer *cp;
	char *buf;
	size_t len;
	g_topology_assert();
	ZFS_LOG(1, "Searching by guids [%ju:%ju].",
	    (uintmax_t)spa_guid(vd->vdev_spa), (uintmax_t)vd->vdev_guid);
	cp = vdev_geom_attach_by_guids(vd);
	if (cp != NULL) {
		len = strlen(cp->provider->name) + strlen("/dev/") + 1;
		buf = kmem_alloc(len, KM_SLEEP);
		snprintf(buf, len, "/dev/%s", cp->provider->name);
		spa_strfree(vd->vdev_path);
		vd->vdev_path = buf;
		ZFS_LOG(1, "Attach by guid [%ju:%ju] succeeded, provider %s.",
		    (uintmax_t)spa_guid(vd->vdev_spa),
		    (uintmax_t)vd->vdev_guid, cp->provider->name);
	} else {
		ZFS_LOG(1, "Search by guid [%ju:%ju] failed.",
		    (uintmax_t)spa_guid(vd->vdev_spa),
		    (uintmax_t)vd->vdev_guid);
	}
	return (cp);
}
static struct g_consumer *
vdev_geom_open_by_path(vdev_t *vd, int check_guid)
{
	struct g_provider *pp;
	struct g_consumer *cp;
	g_topology_assert();
	cp = NULL;
	pp = g_provider_by_name(vd->vdev_path + sizeof ("/dev/") - 1);
	if (pp != NULL) {
		ZFS_LOG(1, "Found provider by name %s.", vd->vdev_path);
		if (!check_guid || vdev_attach_ok(vd, pp) == FULL_MATCH)
			cp = vdev_geom_attach(pp, vd, B_FALSE);
	}
	return (cp);
}
static int
vdev_geom_open(vdev_t *vd, uint64_t *psize, uint64_t *max_psize,
    uint64_t *logical_ashift, uint64_t *physical_ashift)
{
	struct g_provider *pp;
	struct g_consumer *cp;
	int error, has_trim;
	uint16_t rate;
	VERIFY0(tsd_set(zfs_geom_probe_vdev_key, vd));
	if (vd->vdev_path == NULL || strncmp(vd->vdev_path, "/dev/", 5) != 0) {
		vd->vdev_stat.vs_aux = VDEV_AUX_BAD_LABEL;
		return (EINVAL);
	}
	if ((cp = vd->vdev_tsd) != NULL) {
		ASSERT(vd->vdev_reopening);
		goto skip_open;
	}
	DROP_GIANT();
	g_topology_lock();
	error = 0;
	if (vd->vdev_spa->spa_is_splitting ||
	    ((vd->vdev_prevstate == VDEV_STATE_UNKNOWN &&
	    (vd->vdev_spa->spa_load_state == SPA_LOAD_NONE ||
	    vd->vdev_spa->spa_load_state == SPA_LOAD_CREATE)))) {
		cp = vdev_geom_open_by_path(vd, 0);
	} else {
		cp = vdev_geom_open_by_path(vd, 1);
		if (cp == NULL) {
			cp = vdev_geom_open_by_guids(vd);
		}
	}
	VERIFY0(tsd_set(zfs_geom_probe_vdev_key, NULL));
	if (cp == NULL) {
		ZFS_LOG(1, "Vdev %s not found.", vd->vdev_path);
		error = ENOENT;
	} else {
		struct consumer_priv_t *priv;
		struct consumer_vdev_elem *elem;
		int spamode;
		priv = (struct consumer_priv_t *)&cp->private;
		if (cp->private == NULL)
			SLIST_INIT(priv);
		elem = g_malloc(sizeof (*elem), M_WAITOK|M_ZERO);
		elem->vd = vd;
		SLIST_INSERT_HEAD(priv, elem, elems);
		spamode = spa_mode(vd->vdev_spa);
		if (cp->provider->sectorsize > VDEV_PAD_SIZE ||
		    !ISP2(cp->provider->sectorsize)) {
			ZFS_LOG(1, "Provider %s has unsupported sectorsize.",
			    cp->provider->name);
			vdev_geom_close_locked(vd);
			error = EINVAL;
			cp = NULL;
		} else if (cp->acw == 0 && (spamode & FWRITE) != 0) {
			int i;
			for (i = 0; i < 5; i++) {
				error = g_access(cp, 0, 1, 0);
				if (error == 0)
					break;
				g_topology_unlock();
				tsleep(vd, 0, "vdev", hz / 2);
				g_topology_lock();
			}
			if (error != 0) {
				printf("ZFS WARNING: Unable to open %s for "
				    "writing (error=%d).\n",
				    cp->provider->name, error);
				vdev_geom_close_locked(vd);
				cp = NULL;
			}
		}
	}
	if (cp != NULL) {
		vdev_geom_attrchanged(cp, "GEOM::physpath");
		vdev_geom_set_physpath(vd, cp,  B_FALSE);
	}
	g_topology_unlock();
	PICKUP_GIANT();
	if (cp == NULL) {
		vd->vdev_stat.vs_aux = VDEV_AUX_OPEN_FAILED;
		vdev_dbgmsg(vd, "vdev_geom_open: failed to open [error=%d]",
		    error);
		return (error);
	}
skip_open:
	pp = cp->provider;
	*max_psize = *psize = pp->mediasize;
	*logical_ashift = highbit(MAX(pp->sectorsize, SPA_MINBLOCKSIZE)) - 1;
	*physical_ashift = 0;
	if (pp->stripesize && pp->stripesize > (1 << *logical_ashift) &&
	    ISP2(pp->stripesize) && pp->stripeoffset == 0)
		*physical_ashift = highbit(pp->stripesize) - 1;
	vd->vdev_nowritecache = B_FALSE;
	error = g_getattr("GEOM::rotation_rate", cp, &rate);
	if (error == 0 && rate == DISK_RR_NON_ROTATING)
		vd->vdev_nonrot = B_TRUE;
	else
		vd->vdev_nonrot = B_FALSE;
	error = g_getattr("GEOM::candelete", cp, &has_trim);
	vd->vdev_has_trim = (error == 0 && has_trim);
	vd->vdev_has_securetrim = B_FALSE;
	return (0);
}
static void
vdev_geom_close(vdev_t *vd)
{
	struct g_consumer *cp;
	boolean_t locked;
	cp = vd->vdev_tsd;
	DROP_GIANT();
	locked = g_topology_locked();
	if (!locked)
		g_topology_lock();
	if (!vd->vdev_reopening ||
	    (cp != NULL && ((cp->flags & G_CF_ORPHAN) != 0 ||
	    (cp->provider != NULL && cp->provider->error != 0))))
		vdev_geom_close_locked(vd);
	if (!locked)
		g_topology_unlock();
	PICKUP_GIANT();
}
static void
vdev_geom_io_intr(struct bio *bp)
{
	vdev_t *vd;
	zio_t *zio;
	zio = bp->bio_caller1;
	vd = zio->io_vd;
	zio->io_error = bp->bio_error;
	if (zio->io_error == 0 && bp->bio_resid != 0)
		zio->io_error = SET_ERROR(EIO);
	switch (zio->io_error) {
	case ENOTSUP:
		switch (bp->bio_cmd) {
		case BIO_FLUSH:
			vd->vdev_nowritecache = B_TRUE;
			break;
		case BIO_DELETE:
			break;
		}
		break;
	case ENXIO:
		if (!vd->vdev_remove_wanted) {
			if (bp->bio_to->error != 0) {
				vd->vdev_remove_wanted = B_TRUE;
				spa_async_request(zio->io_spa,
				    SPA_ASYNC_REMOVE);
			} else if (!vd->vdev_delayed_close) {
				vd->vdev_delayed_close = B_TRUE;
			}
		}
		break;
	}
	if (zio->io_type != ZIO_TYPE_READ && zio->io_type != ZIO_TYPE_WRITE) {
		g_destroy_bio(bp);
		zio->io_bio = NULL;
	}
	zio_delay_interrupt(zio);
}
struct vdev_geom_check_unmapped_cb_state {
	int	pages;
	uint_t	end;
};
static int
vdev_geom_check_unmapped_cb(void *buf, size_t len, void *priv)
{
	struct vdev_geom_check_unmapped_cb_state *s = priv;
	vm_offset_t off = (vm_offset_t)buf & PAGE_MASK;
	if (s->pages != 0 && off != 0)
		return (1);
	if (s->end != 0)
		return (1);
	s->end = (off + len) & PAGE_MASK;
	s->pages += (off + len + PAGE_MASK) >> PAGE_SHIFT;
	return (0);
}
static int
vdev_geom_check_unmapped(zio_t *zio, struct g_consumer *cp)
{
	struct vdev_geom_check_unmapped_cb_state s;
	if (!unmapped_buf_allowed)
		return (0);
	if (abd_is_linear(zio->io_abd))
		return (0);
	if ((cp->provider->flags & G_PF_ACCEPT_UNMAPPED) == 0)
		return (0);
	s.pages = s.end = 0;
	if (abd_iterate_func(zio->io_abd, 0, zio->io_size,
	    vdev_geom_check_unmapped_cb, &s))
		return (0);
	return (s.pages);
}
static int
vdev_geom_fill_unmap_cb(void *buf, size_t len, void *priv)
{
	struct bio *bp = priv;
	vm_offset_t addr = (vm_offset_t)buf;
	vm_offset_t end = addr + len;
	if (bp->bio_ma_n == 0) {
		bp->bio_ma_offset = addr & PAGE_MASK;
		addr &= ~PAGE_MASK;
	} else {
		ASSERT0(P2PHASE(addr, PAGE_SIZE));
	}
	do {
		bp->bio_ma[bp->bio_ma_n++] =
		    PHYS_TO_VM_PAGE(pmap_kextract(addr));
		addr += PAGE_SIZE;
	} while (addr < end);
	return (0);
}
static void
vdev_geom_io_start(zio_t *zio)
{
	vdev_t *vd;
	struct g_consumer *cp;
	struct bio *bp;
	vd = zio->io_vd;
	switch (zio->io_type) {
	case ZIO_TYPE_IOCTL:
		if (!vdev_readable(vd)) {
			zio->io_error = SET_ERROR(ENXIO);
			zio_interrupt(zio);
			return;
		} else {
			switch (zio->io_cmd) {
			case DKIOCFLUSHWRITECACHE:
				if (zfs_nocacheflush ||
				    vdev_geom_bio_flush_disable)
					break;
				if (vd->vdev_nowritecache) {
					zio->io_error = SET_ERROR(ENOTSUP);
					break;
				}
				goto sendreq;
			default:
				zio->io_error = SET_ERROR(ENOTSUP);
			}
		}
		zio_execute(zio);
		return;
	case ZIO_TYPE_TRIM:
		if (!vdev_geom_bio_delete_disable) {
			goto sendreq;
		}
		zio_execute(zio);
		return;
	default:
			;
	}
sendreq:
	ASSERT(zio->io_type == ZIO_TYPE_READ ||
	    zio->io_type == ZIO_TYPE_WRITE ||
	    zio->io_type == ZIO_TYPE_TRIM ||
	    zio->io_type == ZIO_TYPE_IOCTL);
	cp = vd->vdev_tsd;
	if (cp == NULL) {
		zio->io_error = SET_ERROR(ENXIO);
		zio_interrupt(zio);
		return;
	}
	bp = g_alloc_bio();
	bp->bio_caller1 = zio;
	switch (zio->io_type) {
	case ZIO_TYPE_READ:
	case ZIO_TYPE_WRITE:
		zio->io_target_timestamp = zio_handle_io_delay(zio);
		bp->bio_offset = zio->io_offset;
		bp->bio_length = zio->io_size;
		if (zio->io_type == ZIO_TYPE_READ)
			bp->bio_cmd = BIO_READ;
		else
			bp->bio_cmd = BIO_WRITE;
		int pgs = vdev_geom_check_unmapped(zio, cp);
		if (pgs > 0) {
			bp->bio_ma = malloc(sizeof (struct vm_page *) * pgs,
			    M_DEVBUF, M_WAITOK);
			bp->bio_ma_n = 0;
			bp->bio_ma_offset = 0;
			abd_iterate_func(zio->io_abd, 0, zio->io_size,
			    vdev_geom_fill_unmap_cb, bp);
			bp->bio_data = unmapped_buf;
			bp->bio_flags |= BIO_UNMAPPED;
		} else {
			if (zio->io_type == ZIO_TYPE_READ) {
				bp->bio_data = abd_borrow_buf(zio->io_abd,
				    zio->io_size);
			} else {
				bp->bio_data = abd_borrow_buf_copy(zio->io_abd,
				    zio->io_size);
			}
		}
		break;
	case ZIO_TYPE_TRIM:
		bp->bio_cmd = BIO_DELETE;
		bp->bio_data = NULL;
		bp->bio_offset = zio->io_offset;
		bp->bio_length = zio->io_size;
		break;
	case ZIO_TYPE_IOCTL:
		bp->bio_cmd = BIO_FLUSH;
		bp->bio_data = NULL;
		bp->bio_offset = cp->provider->mediasize;
		bp->bio_length = 0;
		break;
	default:
		panic("invalid zio->io_type: %d\n", zio->io_type);
	}
	bp->bio_done = vdev_geom_io_intr;
	zio->io_bio = bp;
	g_io_request(bp, cp);
}
static void
vdev_geom_io_done(zio_t *zio)
{
	struct bio *bp = zio->io_bio;
	if (zio->io_type != ZIO_TYPE_READ && zio->io_type != ZIO_TYPE_WRITE) {
		ASSERT3P(bp, ==, NULL);
		return;
	}
	if (bp == NULL) {
		ASSERT3S(zio->io_error, ==, ENXIO);
		return;
	}
	if (bp->bio_ma != NULL) {
		free(bp->bio_ma, M_DEVBUF);
	} else {
		if (zio->io_type == ZIO_TYPE_READ) {
			abd_return_buf_copy(zio->io_abd, bp->bio_data,
			    zio->io_size);
		} else {
			abd_return_buf(zio->io_abd, bp->bio_data,
			    zio->io_size);
		}
	}
	g_destroy_bio(bp);
	zio->io_bio = NULL;
}
static void
vdev_geom_hold(vdev_t *vd)
{
}
static void
vdev_geom_rele(vdev_t *vd)
{
}
vdev_ops_t vdev_disk_ops = {
	.vdev_op_init = NULL,
	.vdev_op_fini = NULL,
	.vdev_op_open = vdev_geom_open,
	.vdev_op_close = vdev_geom_close,
	.vdev_op_asize = vdev_default_asize,
	.vdev_op_min_asize = vdev_default_min_asize,
	.vdev_op_min_alloc = NULL,
	.vdev_op_io_start = vdev_geom_io_start,
	.vdev_op_io_done = vdev_geom_io_done,
	.vdev_op_state_change = NULL,
	.vdev_op_need_resilver = NULL,
	.vdev_op_hold = vdev_geom_hold,
	.vdev_op_rele = vdev_geom_rele,
	.vdev_op_remap = NULL,
	.vdev_op_xlate = vdev_default_xlate,
	.vdev_op_rebuild_asize = NULL,
	.vdev_op_metaslab_init = NULL,
	.vdev_op_config_generate = NULL,
	.vdev_op_nparity = NULL,
	.vdev_op_ndisks = NULL,
	.vdev_op_type = VDEV_TYPE_DISK,		 
	.vdev_op_leaf = B_TRUE			 
};
