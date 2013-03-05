

#include <ngx_timer_module.h>

static char *ngx_timer_use(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_timer_create_conf(ngx_cycle_t *cycle);
static char *ngx_timer_init_conf(ngx_cycle_t *cycle, void *conf);
static ngx_int_t ngx_timer_process_init(ngx_cycle_t *cycle);
static void ngx_timer_process_exit(ngx_cycle_t *cycle);

ngx_timer_actions_t   ngx_timer_actions;

static ngx_command_t  ngx_timer_commands[] = {

    {   ngx_string("timer_use"),
        NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
        ngx_timer_use,
        0,
        0,
        NULL
    },

    ngx_null_command
};

static ngx_core_module_t  ngx_timer_module_ctx = {
    ngx_string("timer"),
    ngx_timer_create_conf,
    ngx_timer_init_conf
};

ngx_module_t  ngx_timer_module = {
    NGX_MODULE_V1,
    &ngx_timer_module_ctx,                 /* module context */
    ngx_timer_commands,                    /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    ngx_timer_process_init,                /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    ngx_timer_process_exit,                /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static char *
ngx_timer_sub_module_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_module_t   *m = cmd->post;
    ngx_command_t  *timer_cmd;
    void           *tconf;

    timer_cmd = m->commands;

    for ( /* void */ ; timer_cmd->name.len; timer_cmd++) {

        if (timer_cmd->name.len != cmd->name.len) {
            continue;
        }

        if (ngx_strcmp(timer_cmd->name.data, cmd->name.data) != 0) {
            continue;
        }
#if 0
        if (!(timer_cmd->type & NGX_TIMER_CONF)) {
            continue;
        }
#endif
        tconf = ((void **) cf->ctx)[m->index];

        return timer_cmd->set(cf, timer_cmd, tconf);
    }

    return NGX_CONF_ERROR;
}


static char *
ngx_timer_use(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_timer_conf_t     *old_tcf, *tcf = conf;

    ngx_int_t             m;
    ngx_str_t            *value;
    ngx_timer_module_t   *module;

    if (tcf->use != NGX_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (cf->cycle->old_cycle->conf_ctx) {
        old_tcf = (void *)ngx_get_conf(cf->cycle->old_cycle->conf_ctx,
                                       ngx_timer_module);
    } else {
        old_tcf = NULL;
    }


    for (m = 0; ngx_modules[m]; m++) {
        if (ngx_modules[m]->type != NGX_TIMER_MODULE) {
            continue;
        }

        module = ngx_modules[m]->ctx;
        if (module->name->len == value[1].len) {
            if (ngx_strcmp(module->name->data, value[1].data) == 0) {
                /* different from event_use */
                tcf->use = ngx_modules[m]->index;

                if (ngx_process == NGX_PROCESS_SINGLE
                        && old_tcf
                        && old_tcf->use != tcf->use)
                {
                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                       "when the server runs without a master process "
                                       "the \"%V\" timer type must be the same as "
                                       "in previous configuration - \"%s\" "
                                       "and it cannot be changed on the fly, "
                                       "to change it you need to stop server "
                                       "and start it again",
                                       &value[1], old_tcf->name);

                    return NGX_CONF_ERROR;
                }

                return NGX_CONF_OK;
            }
        }
    }

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid timer type \"%V\"", &value[1]);

    return NGX_CONF_ERROR;
}

static void *
ngx_timer_create_conf(ngx_cycle_t *cycle)
{
    ngx_timer_conf_t     *tcf;
    ngx_uint_t            m;
    ngx_timer_module_t   *module;
    ngx_command_t        *cmd, *commands;
    void                 *rv;
    ngx_uint_t            ngx_timer_max_command;

    ngx_timer_max_command = 0;
    /* copy submodule commands */
    for (m = 0; ngx_modules[m]; m++) {

        if (ngx_modules[m]->type != NGX_TIMER_MODULE) {
            continue;
        }

        if (ngx_modules[m]->commands == NULL) {
            continue;
        }
        for ( cmd = ngx_modules[m]->commands; cmd->name.len; cmd++) {
            ngx_timer_max_command ++;
        }
    }

    if (ngx_timer_max_command != 0) {

        commands = ngx_palloc(cycle->pool,
                              ngx_timer_max_command * sizeof(ngx_command_t) + sizeof(ngx_timer_commands));

        if (commands == NULL) {
            return NULL;
        }


        ngx_timer_module.commands = commands;

        ngx_memcpy(commands, ngx_timer_commands, sizeof(ngx_timer_commands) );

        commands += (sizeof(ngx_timer_commands)/ sizeof(ngx_command_t) - 1);

        for (m = 0; ngx_modules[m]; m++) {

            if (ngx_modules[m]->type != NGX_TIMER_MODULE) {
                continue;
            }

            if (ngx_modules[m]->commands == NULL) {
                continue;
            }

            for (cmd = ngx_modules[m]->commands; cmd->name.len; cmd++) {

                commands->name = cmd->name;
                commands->type = cmd->type;
                commands->set = ngx_timer_sub_module_command;
                commands->conf = cmd->conf;
                commands->offset = 0;
                commands->post = ngx_modules[m];
                commands ++;
            }
        }
        commands->name.len = 0;
        commands->name.data = NULL;
    }


    /* create config  */
    tcf = ngx_pcalloc(cycle->pool, sizeof(ngx_timer_conf_t));
    if (tcf == NULL) {
        return NULL;
    }

    tcf->use= NGX_CONF_UNSET_UINT;

    for (m = 0; ngx_modules[m]; m++) {
        if (ngx_modules[m]->type != NGX_TIMER_MODULE) {
            continue;
        }

        module = ngx_modules[m]->ctx;

        if (module->create_conf) {
            rv = module->create_conf(cycle);
            if (rv == NULL) {
                return NULL;
            }
            cycle->conf_ctx[ngx_modules[m]->index] = rv;
        }
    }

    return tcf;
}

static char *
ngx_timer_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_timer_conf_t      *tcf = conf;
    ngx_int_t              m;
    ngx_timer_module_t    *module;
    char                  *rv;

    if (tcf->use == NGX_CONF_UNSET_UINT) {
        tcf->use = ngx_timer_heap_module.index;
    }

    module = ngx_modules[tcf->use]->ctx;
    tcf->name = module->name->data;
    ngx_timer_actions = module->actions;

    for (m = 0; ngx_modules[m]; m++) {
        if (ngx_modules[m]->type != NGX_TIMER_MODULE) {
            continue;
        }

        module = ngx_modules[m]->ctx;

        if (module->init_conf) {
            rv = module->init_conf(cycle, cycle->conf_ctx[ngx_modules[m]->index]);

            if (rv == NGX_CONF_OK)
            {
                continue;
            }

            return NGX_CONF_ERROR;
        }
    }
    
    ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0,
                  "using the \"%s\" timer module", tcf->name);

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_timer_process_init(ngx_cycle_t *cycle)
{
    if (ngx_timer_actions.init(cycle) != NGX_OK) {
        /* fatal */
        exit(2);
    }

    return NGX_OK;
}

static void
ngx_timer_process_exit(ngx_cycle_t *cycle) {
    ngx_timer_actions.done(cycle);
}

