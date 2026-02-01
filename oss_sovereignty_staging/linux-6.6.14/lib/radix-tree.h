
 

struct kmem_cache;
struct rcu_head;

extern struct kmem_cache *radix_tree_node_cachep;
extern void radix_tree_node_rcu_free(struct rcu_head *head);
