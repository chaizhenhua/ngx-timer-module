

#include <ngx_event.h>
#include <ngx_heap.h>

#define NGX_HEAP_FREE_EVENTS  100
static ngx_thread_volatile ngx_heap_t  ngx_timer_heap;

#define NGX_TREE_INIT(cycle)       ngx_heap_init(&ngx_timer_heap, cycle->pool, \
                                                 cycle->connection_n * 2 + NGX_HEAP_FREE_EVENTS)

#define NGX_TREE_DELETE(timer)     ngx_heap_delete(&ngx_timer_heap, (ngx_heap_node_t *)timer)
#define NGX_TREE_INSERT(timer)     ngx_heap_insert(&ngx_timer_heap, (ngx_heap_node_t *)timer)
#define NGX_TREE_ADJUST(timer)     ngx_heap_adjust(&ngx_timer_heap, (ngx_heap_node_t *)timer)

#define NGX_TREE_MIN()             (void *)ngx_heap_min(&ngx_timer_heap)
#define NGX_TREE_EMPTY()           ngx_heap_empty(&ngx_timer_heap)

#define NGX_TIMER_TREE_PREFIX      ngx_timer_heap

#include <ngx_timer_tree_template.h>

ngx_timer_actions_t  ngx_timer_heap_actions = {
    ngx_string("heap"),
    NGX_TIMER_TREE_ADD,
    NGX_TIMER_TREE_DEL,
    NGX_TIMER_TREE_EMPTY,

    NGX_TIMER_TREE_FIND_MIN,
    NGX_TIMER_TREE_EXPIRE_TIMERS,

    NGX_TIMER_TREE_INIT,
};


