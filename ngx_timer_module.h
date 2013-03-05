
#ifndef _NGX_TIMER_MODULE_INCLUDE_
#define _NGX_TIMER_MODULE_INCLUDE_

#include <ngx_event.h>
#define ngx_timer_before(x, y)  ((ngx_msec_int_t) (x - y) < 0)

typedef struct {
    void  (*add)(ngx_event_t *ev, ngx_msec_t timer);
    void  (*del)(ngx_event_t *ev);
    ngx_int_t  (*empty)(void);
    ngx_msec_t (*expire_time)(void);
    void  (*process_expired)(void);

    ngx_int_t  (*init)(ngx_cycle_t *cycle);
    void       (*done)(ngx_cycle_t *cycle);
} ngx_timer_actions_t;

extern ngx_timer_actions_t   ngx_timer_actions;

typedef struct {
    ngx_str_t              *name;

    void                 *(*create_conf)(ngx_cycle_t *cycle);
    char                 *(*init_conf)(ngx_cycle_t *cycle, void *conf);

    ngx_timer_actions_t     actions;
} ngx_timer_module_t;

#define ngx_add_timer               ngx_timer_actions.add
#define ngx_del_timer               ngx_timer_actions.del
#define ngx_timer_empty             ngx_timer_actions.empty

#define ngx_timer_expire_time       ngx_timer_actions.expire_time
#define ngx_timer_process_expired   ngx_timer_actions.process_expired
#define ngx_done_timers             ngx_timer_actions.done

#include <ngx_event_timer.h>

#define NGX_TIMER_MODULE      0x454d4954  /* "TIME" */
#define NGX_TIMER_CONF        NGX_MAIN_CONF|NGX_DIRECT_CONF

extern ngx_module_t        ngx_timer_module;
extern ngx_module_t        ngx_timer_heap_module;
extern ngx_module_t        ngx_timer_wheel_module;
extern ngx_module_t        ngx_timer_rbtree_module;

#define ngx_timer_get_conf(conf_ctx, module)  (void *)ngx_get_conf(conf_ctx, module)

typedef struct {
    ngx_uint_t     use;
    u_char        *name;
} ngx_timer_conf_t;

#endif /* _NGX_TIMER_MODULE_INCLUDE_ */

