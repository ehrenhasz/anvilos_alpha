#ifndef	_ZFS_PAGE_COMPAT_H
#define	_ZFS_PAGE_COMPAT_H
#if	defined(ZFS_GLOBAL_ZONE_PAGE_STATE)
#if	defined(ZFS_ENUM_NODE_STAT_ITEM_NR_FILE_PAGES)
#define	nr_file_pages() global_node_page_state(NR_FILE_PAGES)
#else
#define	nr_file_pages() global_zone_page_state(NR_FILE_PAGES)
#endif
#if	defined(ZFS_ENUM_NODE_STAT_ITEM_NR_INACTIVE_ANON)
#define	nr_inactive_anon_pages() global_node_page_state(NR_INACTIVE_ANON)
#else
#define	nr_inactive_anon_pages() global_zone_page_state(NR_INACTIVE_ANON)
#endif
#if	defined(ZFS_ENUM_NODE_STAT_ITEM_NR_INACTIVE_FILE)
#define	nr_inactive_file_pages() global_node_page_state(NR_INACTIVE_FILE)
#else
#define	nr_inactive_file_pages() global_zone_page_state(NR_INACTIVE_FILE)
#endif
#elif	defined(ZFS_GLOBAL_NODE_PAGE_STATE)
#if	defined(ZFS_ENUM_NODE_STAT_ITEM_NR_FILE_PAGES)
#define	nr_file_pages() global_node_page_state(NR_FILE_PAGES)
#else
#define	nr_file_pages() global_page_state(NR_FILE_PAGES)
#endif
#if	defined(ZFS_ENUM_NODE_STAT_ITEM_NR_INACTIVE_ANON)
#define	nr_inactive_anon_pages() global_node_page_state(NR_INACTIVE_ANON)
#else
#define	nr_inactive_anon_pages() global_page_state(NR_INACTIVE_ANON)
#endif
#if	defined(ZFS_ENUM_NODE_STAT_ITEM_NR_INACTIVE_FILE)
#define	nr_inactive_file_pages() global_node_page_state(NR_INACTIVE_FILE)
#else
#define	nr_inactive_file_pages() global_page_state(NR_INACTIVE_FILE)
#endif
#else
#define	nr_file_pages()			global_page_state(NR_FILE_PAGES)
#define	nr_inactive_anon_pages()	global_page_state(NR_INACTIVE_ANON)
#define	nr_inactive_file_pages()	global_page_state(NR_INACTIVE_FILE)
#endif  
#endif  
