#include "vglobal.h"
#include "vapp.h"

/*
 *  the routine to enable log stdout.
 *  @app: handle to app.
 */
static
int _vappmain_need_stdout(struct vappmain* app)
{
    vassert(app);
    app->need_stdout = 1;
    return 0;
}

/*
 * the routine indicate this program should be a daemon running backgroud.
 * @app:
 */
static
int _vappmain_need_daemonize(struct vappmain* app)
{
    vassert(app);
    app->daemonized = 1;
    return 0;
}

/*
 * the routine to run the app
 * @app:
 * @cfg_file: the "conf" file.
 */
static
int _vappmain_run(struct vappmain* app, const char* cfg_file)
{
    const char* using_cfg = cfg_file;
    int ret  = 0;
    vassert(app);

    if (!using_cfg) {
        using_cfg = "vdht.conf";
    }

    vlog_open(1, "vdhtd_brew");
    ret = vconfig_init(&app->cfg);
    retE((ret < 0));
    ret = app->cfg.ops->parse(&app->cfg, using_cfg);
    if (ret < 0) {
        goto error_exit;
    }
    ret = vlog_open_with_cfg(&app->cfg);
    if (ret < 0) {
        goto error_exit;
    }
    if (app->need_stdout) {
        vlog_stdout_enable();
    }
    ret = vhost_init(&app->host, &app->cfg);
    if (ret < 0) {
        goto error_exit;
    }
    app->host.ops->stabilize(&app->host);
    app->host.ops->start(&app->host);
    app->host.ops->run(&app->host);

    vhost_deinit(&app->host);
    vconfig_deinit(&app->cfg);
    vlog_close();

error_exit:
    vconfig_deinit(&app->cfg);
    return 0;
}

static
struct vappmain_ops appmain_ops = {
    .need_stdout    = _vappmain_need_stdout,
    .need_daemonize = _vappmain_need_daemonize,
    .run            = _vappmain_run
};

int vappmain_init(struct vappmain* app)
{
    vassert(app);

    app->need_stdout = 0;
    app->daemonized  = 0;

    app->ops = &appmain_ops;
    return 0;
}

void vappmain_deinit(struct vappmain* app)
{
    vassert(app);

    //do nothing;
    return ;
}

