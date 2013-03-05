#include <stdio.h>
#include <ngx_heap.h>
#include <assert.h>
#define PARENT(x)  (x >> 1)
#define CHILD(x)   (x << 1)
#define ROOT       1



/* percolate up */
void
ngx_heap_up (ngx_heap_t *heap, ngx_heap_node_t *node)
{
    int index, par;
    ngx_heap_node_t **array = heap->array;

    for (index = node->index, par = PARENT(index);
            index > 1 && ngx_heap_less(node,array[par]);
            index = par, par = PARENT(index))
    {
        array[index] = array [par];
        array[index]->index = index;
    }

    array [index] = node;
    node->index = index;
}

/* percolate down */
void
ngx_heap_down (ngx_heap_t *heap, ngx_heap_node_t *node)
{
    ngx_uint_t index, child;
    ngx_heap_node_t **array = heap->array;

    for (index = node->index; CHILD(index) + 1 <= heap->last; index = child) {
        child = CHILD(index);
        /* find min child */
        if (ngx_heap_less(array[child + 1], array[child]) ) {
            child ++;
        }

        if (ngx_heap_less(array[child], node) ) {
            array[index] = array[child];
            array[index]->index = index;
        } else {
            break;
        }
    }

    /* last child */
    child = CHILD(index);

    if (child == heap->last && ngx_heap_less(array[child], node) ) {
        array[index] = array[child];
        array[index]->index = index;
        index = child;
    }

    array[index] = node;
    node->index = index;
}

