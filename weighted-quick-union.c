#include <stdlib.h>

#include "weighted-quick-union.h"

int w_quickunion_init(struct wqu_uf *uf, size_t size) {
	
	uf->nodes = malloc(size * sizeof(struct wqu_node));
	if(!uf->nodes) return 0;
	for(size_t i = 0; i < size; i++) {
		uf->nodes[i].id = i;
		uf->nodes[i].size = 1;
	}
	uf->size = size;
	uf->count = size;
	return 1;
}

void w_quickunion_destroy(struct wqu_uf *uf) {
	
	free(uf->nodes);
	uf->nodes = NULL;
	uf->size = 0;
	uf->count = 0;
}

/* static size_t root_id(struct wqu_uf *uf, size_t p) {
	
	struct wqu_node *node = &uf->nodes[p];
	while(node->id != uf->nodes[node->id].id) {
		
		uf->nodes[node->id].id = uf->nodes[uf->nodes[node->id].id].id; // path halving
		node = &uf->nodes[node->id];
	}
	return node->id;
} */

static size_t root_id(struct wqu_uf *uf, size_t p) {
	
	size_t root_i = uf->nodes[p].id;
	while(root_i != uf->nodes[root_i].id)
		root_i = uf->nodes[root_i].id;
	while(p != root_i) {
		size_t newp = uf->nodes[p].id;
		uf->nodes[p].id = root_i;
		p = newp;
	}
	return root_i;
}

bool w_quickunion_is_connected(struct wqu_uf *uf, size_t p, size_t q) {
	
	return root_id(uf, p) == root_id(uf, q);
}

void w_quickunion_union(struct wqu_uf *uf, size_t p, size_t q) {
	
	size_t p_root_id = root_id(uf, p);
	size_t q_root_id = root_id(uf, q);
	if(p_root_id == q_root_id) return;
	struct wqu_node *p_node = &uf->nodes[p];
	struct wqu_node *q_node = &uf->nodes[q];
	
	if(p_node->size <= q_node->size) {
		uf->nodes[p_root_id].id = uf->nodes[q_root_id].id;
		q_node->size += p_node->size;
	}
	else {
		uf->nodes[q_root_id].id = uf->nodes[p_root_id].id;
		p_node->size += q_node->size;
	}
	uf->count --;
}
