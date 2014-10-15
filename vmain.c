#include "vglobal.h"

void show_usage(void)
{
    printf("Usage: vdhtd [OPTION...]\n");
    printf("  -D, --daemon                  Become a daemon (default)\n");
    printf("  -i, --interactive             Run iteractive (not a daemon)\n");
    printf("  -S, --log-stdout              Log out stdout.\n");
    printf("\n");
    printf("Help options\n");
    printf("  -h  --help                    Show this help message.\n");
    printf("  -V, --version                 Print version.\n");
    printf("\n");
    printf("Common vdht options.\n");
    printf("  -f  --configfile=CONFIGFILE   Use alternate configuration file.\n");
    printf("\n");
}

int main(int argc, char** argv)
{
    struct vhost*  host = NULL;
    struct vconfig cfg;
    int ret = 0;

    show_usage();

    vconfig_init(&cfg);
    ret = cfg.ops->parse(&cfg, "vdht.conf");
    if (ret < 0) {
        printf("Failed to parse vdht.config\n");
        vconfig_deinit(&cfg);
        return 0;
    }

    //todo;
    host = vhost_create(&cfg, "10.0.0.16", 12300);
    retE((!host));
    host->ops->stabilize(host);
    host->ops->start(host);

    host->ops->loop(host);

    vhost_destroy(host);
    return 0;
}
