#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/avl.h>
#include <sys/sysmacros.h>
#include "zed_strings.h"
struct zed_strings {
	avl_tree_t tree;
	avl_node_t *iteratorp;
};
struct zed_strings_node {
	avl_node_t node;
	char *key;
	char *val;
};
typedef struct zed_strings_node zed_strings_node_t;
static int
_zed_strings_node_compare(const void *x1, const void *x2)
{
	const char *s1;
	const char *s2;
	int rv;
	assert(x1 != NULL);
	assert(x2 != NULL);
	s1 = ((const zed_strings_node_t *) x1)->key;
	assert(s1 != NULL);
	s2 = ((const zed_strings_node_t *) x2)->key;
	assert(s2 != NULL);
	rv = strcmp(s1, s2);
	if (rv < 0)
		return (-1);
	if (rv > 0)
		return (1);
	return (0);
}
zed_strings_t *
zed_strings_create(void)
{
	zed_strings_t *zsp;
	zsp = calloc(1, sizeof (*zsp));
	if (!zsp)
		return (NULL);
	avl_create(&zsp->tree, _zed_strings_node_compare,
	    sizeof (zed_strings_node_t), offsetof(zed_strings_node_t, node));
	zsp->iteratorp = NULL;
	return (zsp);
}
static void
_zed_strings_node_destroy(zed_strings_node_t *np)
{
	if (!np)
		return;
	if (np->key) {
		if (np->key != np->val)
			free(np->key);
		np->key = NULL;
	}
	if (np->val) {
		free(np->val);
		np->val = NULL;
	}
	free(np);
}
static zed_strings_node_t *
_zed_strings_node_create(const char *key, const char *val)
{
	zed_strings_node_t *np;
	assert(val != NULL);
	np = calloc(1, sizeof (*np));
	if (!np)
		return (NULL);
	np->val = strdup(val);
	if (!np->val)
		goto nomem;
	if (key) {
		np->key = strdup(key);
		if (!np->key)
			goto nomem;
	} else {
		np->key = np->val;
	}
	return (np);
nomem:
	_zed_strings_node_destroy(np);
	return (NULL);
}
void
zed_strings_destroy(zed_strings_t *zsp)
{
	void *cookie;
	zed_strings_node_t *np;
	if (!zsp)
		return;
	cookie = NULL;
	while ((np = avl_destroy_nodes(&zsp->tree, &cookie)))
		_zed_strings_node_destroy(np);
	avl_destroy(&zsp->tree);
	free(zsp);
}
int
zed_strings_add(zed_strings_t *zsp, const char *key, const char *s)
{
	zed_strings_node_t *newp, *oldp;
	if (!zsp || !s) {
		errno = EINVAL;
		return (-1);
	}
	if (key == s)
		key = NULL;
	newp = _zed_strings_node_create(key, s);
	if (!newp)
		return (-1);
	oldp = avl_find(&zsp->tree, newp, NULL);
	if (oldp) {
		avl_remove(&zsp->tree, oldp);
		_zed_strings_node_destroy(oldp);
	}
	avl_add(&zsp->tree, newp);
	return (0);
}
const char *
zed_strings_first(zed_strings_t *zsp)
{
	if (!zsp) {
		errno = EINVAL;
		return (NULL);
	}
	zsp->iteratorp = avl_first(&zsp->tree);
	if (!zsp->iteratorp)
		return (NULL);
	return (((zed_strings_node_t *)zsp->iteratorp)->val);
}
const char *
zed_strings_next(zed_strings_t *zsp)
{
	if (!zsp) {
		errno = EINVAL;
		return (NULL);
	}
	if (!zsp->iteratorp)
		return (NULL);
	zsp->iteratorp = AVL_NEXT(&zsp->tree, zsp->iteratorp);
	if (!zsp->iteratorp)
		return (NULL);
	return (((zed_strings_node_t *)zsp->iteratorp)->val);
}
int
zed_strings_count(zed_strings_t *zsp)
{
	if (!zsp) {
		errno = EINVAL;
		return (-1);
	}
	return (avl_numnodes(&zsp->tree));
}
