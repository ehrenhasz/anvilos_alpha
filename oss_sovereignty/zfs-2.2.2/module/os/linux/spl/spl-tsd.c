 

#include <sys/kmem.h>
#include <sys/thread.h>
#include <sys/tsd.h>
#include <linux/hash.h>

typedef struct tsd_hash_bin {
	spinlock_t		hb_lock;
	struct hlist_head	hb_head;
} tsd_hash_bin_t;

typedef struct tsd_hash_table {
	spinlock_t		ht_lock;
	uint_t			ht_bits;
	uint_t			ht_key;
	tsd_hash_bin_t		*ht_bins;
} tsd_hash_table_t;

typedef struct tsd_hash_entry {
	uint_t			he_key;
	pid_t			he_pid;
	dtor_func_t		he_dtor;
	void			*he_value;
	struct hlist_node	he_list;
	struct list_head	he_key_list;
	struct list_head	he_pid_list;
} tsd_hash_entry_t;

static tsd_hash_table_t *tsd_hash_table = NULL;


 
static tsd_hash_entry_t *
tsd_hash_search(tsd_hash_table_t *table, uint_t key, pid_t pid)
{
	struct hlist_node *node = NULL;
	tsd_hash_entry_t *entry;
	tsd_hash_bin_t *bin;
	ulong_t hash;

	hash = hash_long((ulong_t)key * (ulong_t)pid, table->ht_bits);
	bin = &table->ht_bins[hash];
	spin_lock(&bin->hb_lock);
	hlist_for_each(node, &bin->hb_head) {
		entry = list_entry(node, tsd_hash_entry_t, he_list);
		if ((entry->he_key == key) && (entry->he_pid == pid)) {
			spin_unlock(&bin->hb_lock);
			return (entry);
		}
	}

	spin_unlock(&bin->hb_lock);
	return (NULL);
}

 
static void
tsd_hash_dtor(struct hlist_head *work)
{
	tsd_hash_entry_t *entry;

	while (!hlist_empty(work)) {
		entry = hlist_entry(work->first, tsd_hash_entry_t, he_list);
		hlist_del(&entry->he_list);

		if (entry->he_dtor && entry->he_pid != DTOR_PID)
			entry->he_dtor(entry->he_value);

		kmem_free(entry, sizeof (tsd_hash_entry_t));
	}
}

 
static int
tsd_hash_add(tsd_hash_table_t *table, uint_t key, pid_t pid, void *value)
{
	tsd_hash_entry_t *entry, *dtor_entry, *pid_entry;
	tsd_hash_bin_t *bin;
	ulong_t hash;
	int rc = 0;

	ASSERT3P(tsd_hash_search(table, key, pid), ==, NULL);

	 
	entry = kmem_alloc(sizeof (tsd_hash_entry_t), KM_PUSHPAGE);
	if (entry == NULL)
		return (ENOMEM);

	entry->he_key = key;
	entry->he_pid = pid;
	entry->he_value = value;
	INIT_HLIST_NODE(&entry->he_list);
	INIT_LIST_HEAD(&entry->he_key_list);
	INIT_LIST_HEAD(&entry->he_pid_list);

	spin_lock(&table->ht_lock);

	 
	dtor_entry = tsd_hash_search(table, entry->he_key, DTOR_PID);
	ASSERT3P(dtor_entry, !=, NULL);
	entry->he_dtor = dtor_entry->he_dtor;

	 
	pid_entry = tsd_hash_search(table, PID_KEY, entry->he_pid);
	ASSERT3P(pid_entry, !=, NULL);

	hash = hash_long((ulong_t)key * (ulong_t)pid, table->ht_bits);
	bin = &table->ht_bins[hash];
	spin_lock(&bin->hb_lock);

	 
	hlist_add_head(&entry->he_list, &bin->hb_head);
	list_add(&entry->he_key_list, &dtor_entry->he_key_list);
	list_add(&entry->he_pid_list, &pid_entry->he_pid_list);

	spin_unlock(&bin->hb_lock);
	spin_unlock(&table->ht_lock);

	return (rc);
}

 
static int
tsd_hash_add_key(tsd_hash_table_t *table, uint_t *keyp, dtor_func_t dtor)
{
	tsd_hash_entry_t *tmp_entry, *entry;
	tsd_hash_bin_t *bin;
	ulong_t hash;
	int keys_checked = 0;

	ASSERT3P(table, !=, NULL);

	 
	entry = kmem_alloc(sizeof (tsd_hash_entry_t), KM_PUSHPAGE);
	if (entry == NULL)
		return (ENOMEM);

	 
	spin_lock(&table->ht_lock);
	do {
		 
		if (table->ht_key++ > TSD_KEYS_MAX)
			table->ht_key = 1;

		 
		if (keys_checked++ >= TSD_KEYS_MAX) {
			spin_unlock(&table->ht_lock);
			return (ENOENT);
		}

		tmp_entry = tsd_hash_search(table, table->ht_key, DTOR_PID);
	} while (tmp_entry);

	 
	entry->he_key = *keyp = table->ht_key;
	entry->he_pid = DTOR_PID;
	entry->he_dtor = dtor;
	entry->he_value = NULL;
	INIT_HLIST_NODE(&entry->he_list);
	INIT_LIST_HEAD(&entry->he_key_list);
	INIT_LIST_HEAD(&entry->he_pid_list);

	hash = hash_long((ulong_t)*keyp * (ulong_t)DTOR_PID, table->ht_bits);
	bin = &table->ht_bins[hash];
	spin_lock(&bin->hb_lock);

	hlist_add_head(&entry->he_list, &bin->hb_head);

	spin_unlock(&bin->hb_lock);
	spin_unlock(&table->ht_lock);

	return (0);
}

 
static int
tsd_hash_add_pid(tsd_hash_table_t *table, pid_t pid)
{
	tsd_hash_entry_t *entry;
	tsd_hash_bin_t *bin;
	ulong_t hash;

	 
	entry = kmem_alloc(sizeof (tsd_hash_entry_t), KM_PUSHPAGE);
	if (entry == NULL)
		return (ENOMEM);

	spin_lock(&table->ht_lock);
	entry->he_key = PID_KEY;
	entry->he_pid = pid;
	entry->he_dtor = NULL;
	entry->he_value = NULL;
	INIT_HLIST_NODE(&entry->he_list);
	INIT_LIST_HEAD(&entry->he_key_list);
	INIT_LIST_HEAD(&entry->he_pid_list);

	hash = hash_long((ulong_t)PID_KEY * (ulong_t)pid, table->ht_bits);
	bin = &table->ht_bins[hash];
	spin_lock(&bin->hb_lock);

	hlist_add_head(&entry->he_list, &bin->hb_head);

	spin_unlock(&bin->hb_lock);
	spin_unlock(&table->ht_lock);

	return (0);
}

 
static void
tsd_hash_del(tsd_hash_table_t *table, tsd_hash_entry_t *entry)
{
	hlist_del(&entry->he_list);
	list_del_init(&entry->he_key_list);
	list_del_init(&entry->he_pid_list);
}

 
static tsd_hash_table_t *
tsd_hash_table_init(uint_t bits)
{
	tsd_hash_table_t *table;
	int hash, size = (1 << bits);

	table = kmem_zalloc(sizeof (tsd_hash_table_t), KM_SLEEP);
	if (table == NULL)
		return (NULL);

	table->ht_bins = kmem_zalloc(sizeof (tsd_hash_bin_t) * size, KM_SLEEP);
	if (table->ht_bins == NULL) {
		kmem_free(table, sizeof (tsd_hash_table_t));
		return (NULL);
	}

	for (hash = 0; hash < size; hash++) {
		spin_lock_init(&table->ht_bins[hash].hb_lock);
		INIT_HLIST_HEAD(&table->ht_bins[hash].hb_head);
	}

	spin_lock_init(&table->ht_lock);
	table->ht_bits = bits;
	table->ht_key = 1;

	return (table);
}

 
static void
tsd_hash_table_fini(tsd_hash_table_t *table)
{
	HLIST_HEAD(work);
	tsd_hash_bin_t *bin;
	tsd_hash_entry_t *entry;
	int size, i;

	ASSERT3P(table, !=, NULL);
	spin_lock(&table->ht_lock);
	for (i = 0, size = (1 << table->ht_bits); i < size; i++) {
		bin = &table->ht_bins[i];
		spin_lock(&bin->hb_lock);
		while (!hlist_empty(&bin->hb_head)) {
			entry = hlist_entry(bin->hb_head.first,
			    tsd_hash_entry_t, he_list);
			tsd_hash_del(table, entry);
			hlist_add_head(&entry->he_list, &work);
		}
		spin_unlock(&bin->hb_lock);
	}
	spin_unlock(&table->ht_lock);

	tsd_hash_dtor(&work);
	kmem_free(table->ht_bins, sizeof (tsd_hash_bin_t)*(1<<table->ht_bits));
	kmem_free(table, sizeof (tsd_hash_table_t));
}

 
static void
tsd_remove_entry(tsd_hash_entry_t *entry)
{
	HLIST_HEAD(work);
	tsd_hash_table_t *table;
	tsd_hash_entry_t *pid_entry;
	tsd_hash_bin_t *pid_entry_bin, *entry_bin;
	ulong_t hash;

	table = tsd_hash_table;
	ASSERT3P(table, !=, NULL);
	ASSERT3P(entry, !=, NULL);

	spin_lock(&table->ht_lock);

	hash = hash_long((ulong_t)entry->he_key *
	    (ulong_t)entry->he_pid, table->ht_bits);
	entry_bin = &table->ht_bins[hash];

	 
	pid_entry = list_entry(entry->he_pid_list.next, tsd_hash_entry_t,
	    he_pid_list);

	 
	spin_lock(&entry_bin->hb_lock);
	tsd_hash_del(table, entry);
	hlist_add_head(&entry->he_list, &work);
	spin_unlock(&entry_bin->hb_lock);

	 
	if (pid_entry->he_key == PID_KEY &&
	    list_empty(&pid_entry->he_pid_list)) {
		hash = hash_long((ulong_t)pid_entry->he_key *
		    (ulong_t)pid_entry->he_pid, table->ht_bits);
		pid_entry_bin = &table->ht_bins[hash];

		spin_lock(&pid_entry_bin->hb_lock);
		tsd_hash_del(table, pid_entry);
		hlist_add_head(&pid_entry->he_list, &work);
		spin_unlock(&pid_entry_bin->hb_lock);
	}

	spin_unlock(&table->ht_lock);

	tsd_hash_dtor(&work);
}

 
int
tsd_set(uint_t key, void *value)
{
	tsd_hash_table_t *table;
	tsd_hash_entry_t *entry;
	pid_t pid;
	int rc;
	 
	boolean_t remove = (value == NULL);

	table = tsd_hash_table;
	pid = curthread->pid;
	ASSERT3P(table, !=, NULL);

	if ((key == 0) || (key > TSD_KEYS_MAX))
		return (EINVAL);

	 
	entry = tsd_hash_search(table, key, pid);
	if (entry) {
		entry->he_value = value;
		 
		if (remove)
			tsd_remove_entry(entry);
		return (0);
	}

	 
	if (remove)
		return (0);

	 
	entry = tsd_hash_search(table, PID_KEY, pid);
	if (entry == NULL) {
		rc = tsd_hash_add_pid(table, pid);
		if (rc)
			return (rc);
	}

	rc = tsd_hash_add(table, key, pid, value);
	return (rc);
}
EXPORT_SYMBOL(tsd_set);

 
void *
tsd_get(uint_t key)
{
	tsd_hash_entry_t *entry;

	ASSERT3P(tsd_hash_table, !=, NULL);

	if ((key == 0) || (key > TSD_KEYS_MAX))
		return (NULL);

	entry = tsd_hash_search(tsd_hash_table, key, curthread->pid);
	if (entry == NULL)
		return (NULL);

	return (entry->he_value);
}
EXPORT_SYMBOL(tsd_get);

 
void *
tsd_get_by_thread(uint_t key, kthread_t *thread)
{
	tsd_hash_entry_t *entry;

	ASSERT3P(tsd_hash_table, !=, NULL);

	if ((key == 0) || (key > TSD_KEYS_MAX))
		return (NULL);

	entry = tsd_hash_search(tsd_hash_table, key, thread->pid);
	if (entry == NULL)
		return (NULL);

	return (entry->he_value);
}
EXPORT_SYMBOL(tsd_get_by_thread);

 
void
tsd_create(uint_t *keyp, dtor_func_t dtor)
{
	ASSERT3P(keyp, !=, NULL);
	if (*keyp)
		return;

	(void) tsd_hash_add_key(tsd_hash_table, keyp, dtor);
}
EXPORT_SYMBOL(tsd_create);

 
void
tsd_destroy(uint_t *keyp)
{
	HLIST_HEAD(work);
	tsd_hash_table_t *table;
	tsd_hash_entry_t *dtor_entry, *entry;
	tsd_hash_bin_t *dtor_entry_bin, *entry_bin;
	ulong_t hash;

	table = tsd_hash_table;
	ASSERT3P(table, !=, NULL);

	spin_lock(&table->ht_lock);
	dtor_entry = tsd_hash_search(table, *keyp, DTOR_PID);
	if (dtor_entry == NULL) {
		spin_unlock(&table->ht_lock);
		return;
	}

	 
	while (!list_empty(&dtor_entry->he_key_list)) {
		entry = list_entry(dtor_entry->he_key_list.next,
		    tsd_hash_entry_t, he_key_list);
		ASSERT3U(dtor_entry->he_key, ==, entry->he_key);
		ASSERT3P(dtor_entry->he_dtor, ==, entry->he_dtor);

		hash = hash_long((ulong_t)entry->he_key *
		    (ulong_t)entry->he_pid, table->ht_bits);
		entry_bin = &table->ht_bins[hash];

		spin_lock(&entry_bin->hb_lock);
		tsd_hash_del(table, entry);
		hlist_add_head(&entry->he_list, &work);
		spin_unlock(&entry_bin->hb_lock);
	}

	hash = hash_long((ulong_t)dtor_entry->he_key *
	    (ulong_t)dtor_entry->he_pid, table->ht_bits);
	dtor_entry_bin = &table->ht_bins[hash];

	spin_lock(&dtor_entry_bin->hb_lock);
	tsd_hash_del(table, dtor_entry);
	hlist_add_head(&dtor_entry->he_list, &work);
	spin_unlock(&dtor_entry_bin->hb_lock);
	spin_unlock(&table->ht_lock);

	tsd_hash_dtor(&work);
	*keyp = 0;
}
EXPORT_SYMBOL(tsd_destroy);

 
void
tsd_exit(void)
{
	HLIST_HEAD(work);
	tsd_hash_table_t *table;
	tsd_hash_entry_t *pid_entry, *entry;
	tsd_hash_bin_t *pid_entry_bin, *entry_bin;
	ulong_t hash;

	table = tsd_hash_table;
	ASSERT3P(table, !=, NULL);

	spin_lock(&table->ht_lock);
	pid_entry = tsd_hash_search(table, PID_KEY, curthread->pid);
	if (pid_entry == NULL) {
		spin_unlock(&table->ht_lock);
		return;
	}

	 

	while (!list_empty(&pid_entry->he_pid_list)) {
		entry = list_entry(pid_entry->he_pid_list.next,
		    tsd_hash_entry_t, he_pid_list);
		ASSERT3U(pid_entry->he_pid, ==, entry->he_pid);

		hash = hash_long((ulong_t)entry->he_key *
		    (ulong_t)entry->he_pid, table->ht_bits);
		entry_bin = &table->ht_bins[hash];

		spin_lock(&entry_bin->hb_lock);
		tsd_hash_del(table, entry);
		hlist_add_head(&entry->he_list, &work);
		spin_unlock(&entry_bin->hb_lock);
	}

	hash = hash_long((ulong_t)pid_entry->he_key *
	    (ulong_t)pid_entry->he_pid, table->ht_bits);
	pid_entry_bin = &table->ht_bins[hash];

	spin_lock(&pid_entry_bin->hb_lock);
	tsd_hash_del(table, pid_entry);
	hlist_add_head(&pid_entry->he_list, &work);
	spin_unlock(&pid_entry_bin->hb_lock);
	spin_unlock(&table->ht_lock);

	tsd_hash_dtor(&work);
}
EXPORT_SYMBOL(tsd_exit);

int
spl_tsd_init(void)
{
	tsd_hash_table = tsd_hash_table_init(TSD_HASH_TABLE_BITS_DEFAULT);
	if (tsd_hash_table == NULL)
		return (-ENOMEM);

	return (0);
}

void
spl_tsd_fini(void)
{
	tsd_hash_table_fini(tsd_hash_table);
	tsd_hash_table = NULL;
}
