
#include <ngx_timer_module.h>

static ngx_str_t      timer_rbtree_name = ngx_string("rbtree");
static ngx_int_t ngx_timer_rbtree_init(ngx_cycle_t *cycle);
static void ngx_timer_rbtree_done(ngx_cycle_t *cycle);
static void ngx_timer_rbtree_process_expired(void);
static void ngx_timer_rbtree_add(ngx_event_t *ev, ngx_msec_t timer);
static void ngx_timer_rbtree_del(ngx_event_t *ev);
static ngx_int_t ngx_timer_rbtree_empty(void);
#define ngx_timer_rbtree_expire_time ngx_event_find_timer

ngx_timer_module_t  ngx_timer_rbtree_module_ctx = {
    &timer_rbtree_name,
    NULL,                 /* create configuration */
    NULL,                 /* init configuration */

    {
        ngx_timer_rbtree_add,
        ngx_timer_rbtree_del,
        ngx_timer_rbtree_empty,
        ngx_timer_rbtree_expire_time,
        ngx_timer_rbtree_process_expired,
        ngx_timer_rbtree_init,
        ngx_timer_rbtree_done,
    }
};

ngx_module_t  ngx_timer_rbtree_module = {
    NGX_MODULE_V1,
    &ngx_timer_rbtree_module_ctx,        /* module context */
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

static ngx_int_t
ngx_timer_rbtree_init(ngx_cycle_t *cycle)
{
    return ngx_event_timer_init(cycle->log);
}

static ngx_int_t
ngx_timer_rbtree_empty()
{
    return ngx_event_timer_rbtree.root == ngx_event_timer_rbtree.sentinel;
}

static void
ngx_timer_rbtree_done(ngx_cycle_t *cycle)
{

}

static void
ngx_timer_rbtree_process_expired(void) {
    ngx_event_expire_timers();
}

static void
ngx_timer_rbtree_add(ngx_event_t *ev, ngx_msec_t timer) {
    ngx_event_add_timer(ev, timer);
}
static void
ngx_timer_rbtree_del(ngx_event_t *ev) {
    ngx_event_del_timer(ev);
}
