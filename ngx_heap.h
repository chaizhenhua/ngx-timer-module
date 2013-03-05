#ifndef _NGX_HEAP_INCLUDE_
#define _NGX_HEAP_INCLUDE_
#include <ngx_timer_module.h>
typedef ngx_uint_t  ngx_heap_key_t;

#define ngx_heap_key_less(k1, k2) ngx_timer_before(k1,k2)
#define ngx_heap_less(n1, n2)     ngx_heap_key_less((n1)->key, (n2)->key)
#define ngx_heap_greater(n1, n2) (!ngx_heap_less(n1, n2))

typedef struct ngx_heap_node_s {
    ngx_heap_key_t   key;
    ngx_uint_t       index;
    u_char           data;
} ngx_heap_node_t;

typedef struct ngx_heap_s {
    ngx_heap_node_t **array;
    ngx_uint_t        last;
    ngx_uint_t        max;
} ngx_heap_t;

void ngx_heap_up (ngx_heap_t *heap, ngx_heap_node_t *node);
void ngx_heap_down (ngx_heap_t *heap, ngx_heap_node_t *node);

static inline ngx_int_t
ngx_heap_init(ngx_heap_t *heap, ngx_pool_t *pool, ngx_uint_t max_size)
{
    heap->array = ngx_palloc(pool, max_size * sizeof(ngx_heap_node_t *));
    if (heap->array == NULL) {
        return NGX_ERROR;
    }
    heap->last = 0;
    heap->max = max_size;
    return NGX_OK;
}

static inline ngx_heap_node_t *
ngx_heap_min(ngx_heap_t *heap)
{
    if (heap->last == 0) {
        return NULL;
    }
    return heap->array[1];
}

static inline void
ngx_heap_insert(ngx_heap_t *heap, ngx_heap_node_t *node)
{
    heap->last++;
    /* TODO ASSERT(heap->last < heap->max) */
    heap->array[heap->last] = node;
    node->index = heap->last;
    ngx_heap_up(heap, node);
}

static inline void
ngx_heap_delete(ngx_heap_t *heap, ngx_heap_node_t *node)
{
    ngx_uint_t index = node->index;

    heap->last --;

    if (index < heap->last + 1) {
        heap->array[index] = heap->array[heap->last + 1];
        heap->array[index]->index = index;
        ngx_heap_down(heap, heap->array[index]);
    }
}

static inline void
ngx_heap_reset_key(ngx_heap_t *heap, ngx_heap_node_t *node, ngx_heap_key_t key)
{
    if (ngx_heap_key_less(node->key, key)) {
        /* increase key */
        node->key = key;
        ngx_heap_down(heap, node);
    } else {
        /* descrease key */
        node->key = key;
        ngx_heap_up(heap, node);
    }
}


#endif /* _NGX_HEAP_INCLUDE_ */
