
#include <ngx_timer_module.h>
#include <ngx_heap.h>

#define NGX_HEAP_FREE_EVENTS  100

static ngx_str_t      timer_heap_name = ngx_string("heap");
static ngx_int_t ngx_timer_heap_init(ngx_cycle_t *cycle);
static void ngx_timer_heap_done(ngx_cycle_t *cycle);
static ngx_msec_t ngx_timer_heap_expire_time(void);
static void ngx_timer_heap_process_expired(void);
static void ngx_timer_heap_add_timer(ngx_event_t *ev, ngx_msec_t timer);
static void ngx_timer_heap_del_timer(ngx_event_t *ev);
static ngx_int_t ngx_timer_heap_empty();

ngx_timer_module_t  ngx_timer_heap_module_ctx = {
    &timer_heap_name,
    NULL,                                     /* create configuration */
    NULL,                                     /* init configuration */

    {
        ngx_timer_heap_add_timer,             /* add timer */
        ngx_timer_heap_del_timer,             /* delete timer */
        ngx_timer_heap_empty,                 /* has any timer? */

        ngx_timer_heap_expire_time,
        ngx_timer_heap_process_expired,       /* process expired */

        ngx_timer_heap_init,                  /* init the timer */
        ngx_timer_heap_done,                  /* done the timer */
    }
};

ngx_module_t  ngx_timer_heap_module = {
    NGX_MODULE_V1,
    &ngx_timer_heap_module_ctx,          /* module context */
    NULL,                                /* module directives */
    NGX_TIMER_MODULE,                    /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};

ngx_thread_volatile ngx_heap_t  ngx_timer_heap;

static ngx_int_t
ngx_timer_heap_init(ngx_cycle_t *cycle)
{
    ngx_int_t              rc;

    rc = ngx_event_timer_init(cycle->log);
    if (rc != NGX_OK) {
        return rc;
    }

    return ngx_heap_init(&ngx_timer_heap, cycle->pool, cycle->connection_n * 2 + NGX_HEAP_FREE_EVENTS);
}

static void
ngx_timer_heap_done(ngx_cycle_t *cycle)
{

}

ngx_msec_t ngx_timer_heap_expire_time(void)
{
    ngx_heap_node_t    *node;
    ngx_msec_int_t      timer;

    ngx_mutex_lock(ngx_event_timer_mutex);
    node = ngx_heap_min(&ngx_timer_heap);

    if(node == NULL) {
        ngx_mutex_unlock(ngx_event_timer_mutex);
        return NGX_TIMER_INFINITE;
    }

    timer = (ngx_msec_int_t) (node->key - ngx_current_msec);
    ngx_mutex_unlock(ngx_event_timer_mutex);

    return (ngx_msec_t) (timer > 0 ? timer : 0);
}


static void
ngx_timer_heap_process_expired(void)
{
    ngx_event_t        *ev;
    ngx_heap_node_t    *node;

    for ( ;; ) {

        ngx_mutex_lock(ngx_event_timer_mutex);

        node = ngx_heap_min(&ngx_timer_heap);

        if (node == NULL) {
            break;
        }

        /* node->key <= ngx_current_time */
        if ((ngx_msec_int_t) (node->key - ngx_current_msec) <= 0) {
            ev = (ngx_event_t *) ((char *) node - offsetof(ngx_event_t, timer));

#if (NGX_THREADS)

            if (ngx_threaded && ngx_trylock(ev->lock) == 0) {

                /*
                 * We cannot change the timer of the event that is being
                 * handled by another thread.  And we cannot easy walk
                 * the rbtree to find next expired timer so we exit the loop.
                 * However, it should be a rare case when the event that is
                 * being handled has an expired timer.
                 */

                ngx_log_debug1(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                               "event %p is busy in expire timers", ev);
                break;
            }
#endif

            ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                           "timer heap del: %d: %M",
                           ngx_event_ident(ev->data), ev->timer.key);

            ngx_heap_delete(&ngx_timer_heap, (ngx_heap_node_t *)&ev->timer);

            ngx_mutex_unlock(ngx_event_timer_mutex);

            ev->timer_set = 0;

#if (NGX_THREADS)
            if (ngx_threaded) {
                ev->posted_timedout = 1;

                ngx_post_event(ev, &ngx_posted_events);

                ngx_unlock(ev->lock);

                continue;
            }
#endif

            ev->timedout = 1;

            ev->handler(ev);

            continue;
        }

        break;
    }

    ngx_mutex_unlock(ngx_event_timer_mutex);

}


static void
ngx_timer_heap_add_timer(ngx_event_t *ev, ngx_msec_t timer)
{
    ngx_msec_t             key;
    ngx_heap_node_t       *node;

    key = timer + ngx_current_msec;
    node = (ngx_heap_node_t *) &ev->timer;

    if (ev->timer_set) {

        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                       "timer heap mod: %d: %M:%M",
                       ngx_event_ident(ev->data), timer, key);

        ngx_mutex_lock(ngx_event_timer_mutex);
        ngx_heap_reset_key(&ngx_timer_heap, node, key);
        ngx_mutex_unlock(ngx_event_timer_mutex);
        return;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "timer heap add: %d: %M:%M",
                   ngx_event_ident(ev->data), timer, key);

    node->key = key;
    ngx_mutex_lock(ngx_event_timer_mutex);
    ngx_heap_insert(&ngx_timer_heap, node);
    ngx_mutex_unlock(ngx_event_timer_mutex);

    ev->timer_set = 1;
}

static void
ngx_timer_heap_del_timer(ngx_event_t *ev)
{
    ngx_heap_node_t       *node;

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "timer heap del: %d: %M",
                   ngx_event_ident(ev->data), ev->timer.key);

    node = (ngx_heap_node_t *) &ev->timer;

    ngx_mutex_lock(ngx_event_timer_mutex);
    ngx_heap_delete(&ngx_timer_heap, node);
    ngx_mutex_unlock(ngx_event_timer_mutex);

    ev->timer_set = 0;
}

static ngx_int_t
ngx_timer_heap_empty()
{
    return ngx_timer_heap.last == 0;
}

