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
 */
static
int _vappmain_run(struct vappmain* app)
{
    const char* pid_file  = NULL;
    int ret = 0;
    vassert(app);

    {
        struct stat sbuf;
        char buf[8];
        int fd  = 0;

        pid_file = app->cfg.ext_ops->get_pid_filename(&app->cfg);
        ret = stat(pid_file, &sbuf);
        vlogEv((ret >= 0), "vdhtd is already running");
        retE((ret >= 0));

        vlogEv((errno != ENOENT), elog_stat);
        retE((errno != ENOENT));

        fd = open(pid_file, O_CLOEXEC|O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
        vlogEv((fd < 0), elog_open);
        retE((fd < 0));
        memset(buf, 0, 8);
        sprintf(buf, "%d", getpid());
        write(fd, buf, strlen(buf));
        close(fd);
    }
    ret = vlog_open_with_cfg(&app->cfg);
    if (ret < 0) {
        unlink(pid_file);
        retE((1));
    }
    if (app->need_stdout) {
        vlog_stdout_enable();
    }
    app->host.ops->stabilize(&app->host);
    app->host.ops->start(&app->host);
    app->host.ops->run(&app->host);

    unlink(pid_file);
    return 0;
}

static
struct vappmain_ops appmain_ops = {
    .need_stdout    = _vappmain_need_stdout,
    .need_daemonize = _vappmain_need_daemonize,
    .run            = _vappmain_run
};

int vappmain_init(struct vappmain* app, const char* cfg_file)
{
    const char* using_cfg = cfg_file;
    int ret = 0;
    vassert(app);

    app->need_stdout = 0;
    app->daemonized  = 0;

    vlog_open(1, "vdhtd_brew");
    vlog_stdout_enable();

    if (!using_cfg) {
        using_cfg = "vdht.conf";
    }
    vconfig_init(&app->cfg);
    ret = app->cfg.ops->parse(&app->cfg, using_cfg);
    retE((ret < 0));

    ret = vhost_init(&app->host, &app->cfg);
    if (ret < 0) {
        vconfig_deinit(&app->cfg);
        retE((1));
    }
    vlog_stdout_disable();
    app->ops = &appmain_ops;
    return 0;
}

void vappmain_deinit(struct vappmain* app)
{
    vassert(app);
    //do nothing;
    vconfig_deinit(&app->cfg);
    vlog_close();
    return ;
}

