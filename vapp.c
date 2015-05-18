#include "vglobal.h"
#include "vapp.h"

static
int _aux_appmain_prepare(const char* pid_file)
{
    struct stat sbuf;
    char buf[32];
    int fd  = 0;
    int pid = 0;
    int ret = 0;

    vassert(pid_file);

    fd = open(pid_file, O_RDONLY);
    if (fd > 0) {
        memset(buf, 0, 32);
        ret = read(fd, buf, 32);
        close(fd);
        vlogEv((ret < 0), elog_read);
        retE((ret < 0));
        errno = 0;
        pid = strtol(buf, NULL, 10);
        vlogEv((errno), elog_strtol);
        retE((errno));
        retE((pid < 0));

        memset(buf, 0, 32);
        sprintf(buf, "/proc/%d", pid);
        errno = 0;
        ret = stat(buf, &sbuf);
        if (ret >= 0) {
            vlogE("vdhtd is already running");
            retE((1));
        }else if (errno == ENOENT) {
            unlink(pid_file);
        }else {
            vlogE(elog_stat);
            retE((1));
        }
    }

    fd = open(pid_file, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
    vlogEv((fd < 0), elog_open);
    retE((fd < 0));
    memset(buf, 0, 32);
    sprintf(buf, "%d", getpid());
    write(fd, buf, strlen(buf));
    close(fd);

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

    if (!app) {
        vlogE("invalid app handle");
        retE((1));
    }

    pid_file = app->cfg.ext_ops->get_pid_filename(&app->cfg);
    ret = _aux_appmain_prepare(pid_file);
    retE((ret < 0));

    ret = vlog_open_with_cfg(&app->cfg);
    if (ret < 0) {
        unlink(pid_file);
        retE((1));
    }
    app->host.ops->stabilize(&app->host);
    app->host.ops->start(&app->host);
    app->host.ops->run(&app->host);

    unlink(pid_file);
    return 0;
}

static
struct vappmain_ops appmain_ops = {
    .run = _vappmain_run
};

static
int _vappmain_start_host(struct vappmain* app)
{
    struct vhost* host = &app->host;
    int ret = 0;

    vassert(app);

    ret = host->ops->start(host);
    retE((ret < 0));
    return 0;
}

static
int _vappmain_stop_host(struct vappmain* app)
{
    struct vhost* host = &app->host;
    int ret = 0;

    vassert(app);

    ret = host->ops->stop(host);
    retE((ret < 0));
    return 0;
}

static
void _vappmain_make_host_exit(struct vappmain* app)
{
    struct vhost* host = &app->host;
    int ret = 0;

    vassert(app);

    ret = host->ops->exit(host);
    retE_v((ret < 0));
    return ;
}

static
void _vappmain_dump_host_info(struct vappmain* app)
{
    struct vhost* host = &app->host;
    vassert(app);

    host->ops->dump(host);
    return;
}

static
void _vappmain_dump_cfg_info(struct vappmain* app)
{
    struct vconfig* cfg = &app->cfg;
    vassert(app);

    cfg->ops->dump(cfg);
    return ;
}

static
int _vappmain_join_wellknown_node(struct vappmain* app, struct sockaddr_in* wellknown_addr)
{
    struct vhost* host = &app->host;
    int ret = 0;

    vassert(app);
    if (!wellknown_addr) {
        vlogE("invalid argument");
        retE((1));
    }
    ret = host->ops->join(host, wellknown_addr);
    retE((ret < 0));
    return 0;
}

static
int _vappmain_bogus_query(struct vappmain* app, int queryId, struct sockaddr_in* wellknown_addr)
{
    struct vhost* host = &app->host;
    int ret = 0;

    vassert(app);
    if (!wellknown_addr) {
        vlogE("invalid argument");
        retE((1));
    }
    if ((queryId != VDHT_PING) &&
        (queryId != VDHT_FIND_NODE) &&
        (queryId != VDHT_FIND_CLOSEST_NODES)) {
        vlogE("unsupported bogus query ID");
        retE((1));
    }
    ret = host->ops->bogus_query(host, queryId, wellknown_addr);
    retE((ret < 0));
    return 0;
}

static
int _vappmain_post_service_segment(struct vappmain* app, vsrvcHash* srvcHash, struct sockaddr_in* addr, int proto)
{
    struct vnode* node = &app->host.node;
    int ret = 0;

    if (!srvcHash || !addr) {
        vlogE("invalid arguments");
        retE((1));
    }
    ret = node->srvc_ops->post(node, srvcHash, addr, proto);
    retE((ret < 0));
    return 0;
}

static
int _vappmain_post_service(struct vappmain* app, vsrvcHash* srvcHash, struct sockaddr_in* addrs, int num, int proto)
{
    struct vnode* node = &app->host.node;
    int ret = 0;
    int i = 0;

    if (!srvcHash || !addrs || num <= 0) {
        vlogE("invalid arguments");
        retE((1));
    }
    for (i = 0; i < num; i++) {
        ret = node->srvc_ops->post(node, srvcHash, &addrs[i], proto);
        retE((ret < 0));
    }
    return 0;
}

static
int _vappmain_unpost_service_segment(struct vappmain* app, vsrvcHash* srvcHash, struct sockaddr_in* addr)
{
    struct vnode* node = &app->host.node;
    int ret = 0;

    if (!srvcHash || !addr) {
        vlogE("invalid arguments");
        retE((1));
    }
    ret = node->srvc_ops->unpost(node, srvcHash, addr);
    retE((ret < 0));
    return 0;
}

static
int _vappmain_unpost_service(struct vappmain* app, vsrvcHash* srvcHash)
{
    struct vnode* node = &app->host.node;
    int ret = 0;

    if (!srvcHash) {
        vlogE("invalid arguments");
        retE((1));
    }
    ret = node->srvc_ops->unpost_ext(node, srvcHash);
    retE((ret < 0));
    return 0;
}

static
int _vappmain_find_service(struct vappmain* app, vsrvcHash* srvcHash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct vnode* node = &app->host.node;
    int ret = 0;

    if (!srvcHash || !ncb || !icb) {
        vlogE("invalid arguments");
        retE((1));
    }
    ret = node->srvc_ops->find(node, srvcHash, ncb, icb, cookie);
    retE((ret < 0));
    return 0;
}

static
int _vappmain_probe_service(struct vappmain* app, vsrvcHash* srvcHash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct vnode* node = &app->host.node;
    int ret = 0;

    if (!srvcHash || !ncb || !icb) {
        vlogE("invalid arguments");
        retE((1));
    }
    ret = node->srvc_ops->probe(node, srvcHash, ncb, icb, cookie);
    retE((ret < 0));
    return 0;
}

static
const char* _vappmain_get_version(struct vappmain* app)
{
    vassert(app);
    return vhost_get_version();
}

static
struct vappmain_api_ops appmain_api_ops = {
    .start_host             = _vappmain_start_host,
    .stop_host              = _vappmain_stop_host,
    .make_host_exit         = _vappmain_make_host_exit,
    .dump_host_info         = _vappmain_dump_host_info,
    .dump_cfg_info          = _vappmain_dump_cfg_info,
    .join_wellknown_node    = _vappmain_join_wellknown_node,
    .bogus_query            = _vappmain_bogus_query,
    .post_service_segment   = _vappmain_post_service_segment,
    .post_service           = _vappmain_post_service,
    .unpost_service_segment = _vappmain_unpost_service_segment,
    .unpost_service         = _vappmain_unpost_service,
    .find_service           = _vappmain_find_service,
    .probe_service          = _vappmain_probe_service,
    .get_version            = _vappmain_get_version
};

int vappmain_init(struct vappmain* app, const char* cfg_file, int stdout)
{
    int ret = 0;
    vassert(app);

    if (!cfg_file) {
        vlogE("missing config file.");
        retE((1));
    }
    if (stdout) {
        vlog_stdout_enable();
    }
    vlog_open(1, "vdhtd_brew");

    vconfig_init(&app->cfg);
    ret = app->cfg.ops->parse(&app->cfg, cfg_file);
    if (ret < 0) {
        vconfig_deinit(&app->cfg);
        retE((1));
    }
    ret = vlsctl_init(&app->lsctl, app, &app->cfg);
    if (ret < 0) {
        vconfig_deinit(&app->cfg);
        retE((1));
    }
    ret = vhost_init(&app->host, &app->cfg, &app->lsctl);
    if (ret < 0) {
        vconfig_deinit(&app->cfg);
        vlsctl_deinit(&app->lsctl);
        retE((1));
    }
    app->ops = &appmain_ops;
    app->api_ops = &appmain_api_ops;
    return 0;
}

void vappmain_deinit(struct vappmain* app)
{
    vassert(app);

    vconfig_deinit(&app->cfg);
    vlsctl_deinit(&app->lsctl);
    vhost_deinit(&app->host);
    vlog_close();
    return ;
}

