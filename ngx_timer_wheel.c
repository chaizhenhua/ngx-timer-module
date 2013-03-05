
#include <ngx_timer_module.h>

static ngx_str_t      timer_wheel_name = ngx_string("wheel");

#define NGX_TIMER_WHEEL_DEFAULT_MAX_MSEC 4*60*1000 /* 4 mins */



typedef struct {
    ngx_msec_t          max_msec;
} ngx_timer_wheel_conf_t;

typedef ngx_msec_t   ngx_timer_wheel_key_t;

typedef struct ngx_timer_wheel_node_s ngx_timer_wheel_node_t;
struct ngx_timer_wheel_node_s {
    ngx_timer_wheel_key_t     key;
    ngx_timer_wheel_node_t   *next;
    ngx_timer_wheel_node_t  **prev;

    u_char                    data;
};

typedef struct ngx_timer_wheel_s {
    ngx_timer_wheel_node_t **wheel;
    ngx_uint_t               size;
    ngx_uint_t               wheel_size;
    ngx_msec_t               resolution;
    ngx_uint_t               slot;
    ngx_timer_wheel_node_t  *pending;
} ngx_timer_wheel_t;

static void *ngx_timer_wheel_create_conf(ngx_cycle_t *cycle);
static char *ngx_timer_wheel_init_conf(ngx_cycle_t *cycle, void *conf);
static ngx_int_t ngx_timer_wheel_init(ngx_cycle_t *cycle);
static void ngx_timer_wheel_done(ngx_cycle_t *cycle);
static void ngx_timer_wheel_process_expired(void);
static ngx_msec_t ngx_timer_wheel_expire_time(void);
static void ngx_timer_wheel_add(ngx_event_t *ev, ngx_msec_t timer);
static void ngx_timer_wheel_del(ngx_event_t *ev);
static ngx_int_t ngx_timer_wheel_empty(void);
static inline void ngx_timer_wheel_process_queue(
    ngx_timer_wheel_node_t  *head, ngx_uint_t pending);

static ngx_command_t  ngx_timer_wheel_commands[] = {

    {   ngx_string("timer_wheel_max"),
        NGX_TIMER_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_msec_slot,
        0,
        offsetof(ngx_timer_wheel_conf_t, max_msec),
        NULL
    },

    ngx_null_command
};

ngx_timer_module_t  ngx_timer_wheel_module_ctx = {
    &timer_wheel_name,
    ngx_timer_wheel_create_conf,               /* create configuration */
    ngx_timer_wheel_init_conf,                 /* init configuration */

    {
        ngx_timer_wheel_add,                   /* add timer */
        ngx_timer_wheel_del,                   /* delete timer */
        ngx_timer_wheel_empty,

        ngx_timer_wheel_expire_time,
        ngx_timer_wheel_process_expired,       /* process timeouts */
        ngx_timer_wheel_init,                  /* init timer */
        ngx_timer_wheel_done,                  /* done */
    }
};

ngx_module_t  ngx_timer_wheel_module = {
    NGX_MODULE_V1,
    &ngx_timer_wheel_module_ctx,         /* module context */
    ngx_timer_wheel_commands,            /* module directives */
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



static void *
ngx_timer_wheel_create_conf(ngx_cycle_t *cycle)
{
    ngx_timer_wheel_conf_t  *cf;

    cf = ngx_palloc(cycle->pool, sizeof(ngx_timer_wheel_conf_t));
    if (cf == NULL) {
        return NULL;
    }

    cf->max_msec = NGX_CONF_UNSET_MSEC;

    return cf;
}



static char *
ngx_timer_wheel_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_timer_wheel_conf_t *cf = conf;
    ngx_core_conf_t        *ccf;
    ngx_timer_conf_t       *tcf;

    tcf = ngx_timer_get_conf(cycle->conf_ctx, ngx_timer_module);
    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    if (tcf->use == ngx_timer_wheel_module.index) {

        if (ccf->timer_resolution == 0) {

            ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                          "when use \"wheel\" timer type, timer resolution must be specified");

            return NGX_CONF_ERROR;
        }
    }

    ngx_conf_init_msec_value(cf->max_msec, NGX_TIMER_WHEEL_DEFAULT_MAX_MSEC);

    if (cf->max_msec < ccf->timer_resolution * 1024) {
        cf->max_msec = ccf->timer_resolution * 1024;
    }

    return NGX_CONF_OK;
}



ngx_thread_volatile ngx_timer_wheel_t  ngx_timer_wheel;

/*
#if (NGX_HAVE_TIMERFD)
static int ngx_timer_wheel_fd;

static void
ngx_timer_wheel_timerfd_handler(ngx_event_t *ev)
{
    ngx_int_t          n;
    ngx_connection_t  *c;
    uint64_t           step;

    c = ev->data;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ev->log, 0, "timer wheel timerfd handler");

    n = ngx_read_fd(c->fd, &step, sizeof(step));

    if (n == NGX_AGAIN) {
        return;
    }

    (void) ngx_timer_wheel_advance((ngx_uint_t) step);
}
#endif
*/


#define ngx_timer_wheel_slot(key) (((key + ngx_timer_wheel.resolution - 1) / ngx_timer_wheel.resolution) % ngx_timer_wheel.wheel_size)

#define ngx_timer_wheel_unlink(node) \
                                                                              \
    *(node->prev) = node->next;                                               \
                                                                              \
    if (node->next) {                                                         \
        node->next->prev = node->prev;                                        \
    }                                                                         \
                                                                              \
    node->prev = NULL;                                                        \
 

#define ngx_timer_wheel_link(queue, node)                                 \
                                                                          \
    if (node->prev != NULL) {                                             \
        ngx_timer_wheel_unlink(node);                                     \
    }                                                                     \
    node->next = (ngx_timer_wheel_node_t *) *queue;                       \
    node->prev = (ngx_timer_wheel_node_t **) queue;                       \
    *queue = node;                                                        \
                                                                          \
    if (node->next) {                                                     \
        node->next->prev = &node->next;                                   \
    }

static ngx_int_t
ngx_timer_wheel_init(ngx_cycle_t *cycle)
{
    ngx_timer_wheel_conf_t *cf;
    ngx_core_conf_t        *ccf;

    cf = ngx_timer_get_conf(cycle->conf_ctx, ngx_timer_wheel_module);
    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    ngx_timer_wheel.resolution =  ccf->timer_resolution;
    ngx_timer_wheel.wheel_size = cf->max_msec / ngx_timer_wheel.resolution;
    ngx_timer_wheel.slot = ngx_timer_wheel_slot(ngx_current_msec);
    ngx_timer_wheel.wheel = ngx_pcalloc(cycle->pool, ngx_timer_wheel.wheel_size * sizeof(ngx_timer_wheel_node_t *));
    ngx_timer_wheel.size = 0;

    if (ngx_timer_wheel.wheel == NULL) {
        return NGX_ERROR;
    }



    /*
    #if (NGX_HAVE_TIMERFD)
        if (cf->use_timerfd) {

            ngx_timer_wheel_fd = timerfd_create(CLOCK_REALTIME, 0);
            if (ngx_timer_wheel_fd == -1) {
                return NGX_ERROR;
            }

            struct itimerspec itv;

            itv.it_interval.tv_sec = itv.it_value.tv_sec = ngx_timer_wheel.resolution/1000;
            itv.it_interval.tv_nsec= itv.it_value.tv_nsec= (ngx_timer_wheel.resolution%1000) * 1000;

            if (timerfd_settime(ngx_timer_wheel_fd, 0, &itv, NULL) == -1) {
                return NGX_ERROR;
            }
            if (ngx_add_channel_event(cycle, ngx_timer_wheel_fd, NGX_READ_EVENT, ngx_timer_wheel_timerfd_handler)
                != NGX_OK) {
                return NGX_ERROR;
            }
        }
    #endif
    */
    return NGX_OK;
}

static void
ngx_timer_wheel_done(ngx_cycle_t *cycle)
{

}

static void
ngx_timer_wheel_process_expired(void) {

    ngx_uint_t step;
    /* prev slot of current time */
    step = (ngx_current_msec/ngx_timer_wheel.resolution) % ngx_timer_wheel.wheel_size;

    ngx_mutex_lock(ngx_event_timer_mutex);

    ngx_timer_wheel_process_queue(ngx_timer_wheel.pending, 1);

    while (ngx_timer_wheel.slot <= step) {
        ngx_timer_wheel_process_queue(ngx_timer_wheel.wheel[ngx_timer_wheel.slot], 0);
        ngx_timer_wheel.slot ++;
    }

    ngx_timer_wheel_process_queue(ngx_timer_wheel.pending, 1);

    ngx_mutex_unlock(ngx_event_timer_mutex);
}



static inline void
ngx_timer_wheel_process_queue(ngx_timer_wheel_node_t  *head, ngx_uint_t pending)
{
    ngx_timer_wheel_node_t *node;
    ngx_event_t            *ev;

    while (head) {

        node = head;
        head = node->next;

        ev = (ngx_event_t *) ((char *) node - offsetof(ngx_event_t, timer));

        if (ngx_timer_before(ngx_current_msec, node->key)) {
            continue;
        }

#if (NGX_THREADS)
        if (ngx_threaded && ngx_trylock(ev->lock) == 0) {

            if (pending) {
                ngx_log_debug1(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                               "event %p is still busy in expire timers", ev);
                continue;
            }

            ngx_timer_wheel_unlink(node);
            ngx_log_debug1(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                           "event %p is busy in expire timers move to pending queue", ev);
            ngx_timer_wheel_link(&ngx_timer_wheel.pending, node);
            continue;
        }
#endif

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                       "timer wheel del: %d: %M",
                       ngx_event_ident(ev->data), ev->timer.key);

        ngx_timer_wheel_unlink(node);
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
    }
}

static ngx_msec_t
ngx_timer_wheel_expire_time(void)
{
    return ngx_timer_wheel.resolution;
}

static void
ngx_timer_wheel_add(ngx_event_t *ev, ngx_msec_t timer)
{
    ngx_msec_t      key;
    ngx_timer_wheel_node_t **queue, *node;

    if (ev->timer_set) {
        ngx_timer_wheel_del(ev);
    }

    key = ngx_current_msec + timer;
    ev->timer.key = key;
    node = (void *)&ev->timer;

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "timer wheel add: %d: %M:%M",
                   ngx_event_ident(ev->data), timer, ev->timer.key);

    ngx_mutex_lock(ngx_event_timer_mutex);

    queue = &ngx_timer_wheel.wheel[ngx_timer_wheel_slot(key)];

    ngx_timer_wheel_link(queue, node);

    ngx_timer_wheel.size ++;

    ngx_mutex_unlock(ngx_event_timer_mutex);

    ev->timer_set = 1;
}



static void
ngx_timer_wheel_del(ngx_event_t *ev)
{
    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "timer wheel del: %d: %M",
                   ngx_event_ident(ev->data), ev->timer.key);

    ngx_mutex_lock(ngx_event_timer_mutex);

    ngx_timer_wheel_unlink(((ngx_timer_wheel_node_t *)&ev->timer) );

    ngx_timer_wheel.size --;

    ngx_mutex_unlock(ngx_event_timer_mutex);

    ev->timer_set = 0;
}

static ngx_int_t
ngx_timer_wheel_empty(void)
{
    return ngx_timer_wheel.size == 0;
}

