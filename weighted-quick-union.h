#if !defined(WEIGHTED_QUICK_UNION_H)
#define WEIGHTED_QUICK_UNION_H

#include <stddef.h>
#include <stdbool.h>


struct wqu_node {	
	size_t id;
	size_t size; // number of nodes of the component this node is connected to
};

struct wqu_uf {
	struct wqu_node *nodes;
	size_t size;
	size_t count;
};

int w_quickunion_init(struct wqu_uf *uf, size_t size);

void w_quickunion_destroy(struct wqu_uf *uf);

bool w_quickunion_is_connected(struct wqu_uf *uf, size_t p, size_t q);

void w_quickunion_union(struct wqu_uf *uf, size_t p, size_t q);

#endif /* WEIGHTED_QUICK_UNION_H */
