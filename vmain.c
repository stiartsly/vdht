#include "vglobal.h"

int main(int argc, char** argv)
{
    struct vhost* host = NULL;

    host = vhost_create("10.0.0.98", 12300);
    retE((!host));
    host->ops->stabilize(host);
    host->ops->start(host);

    host->ops->loop(host);

    vhost_destroy(host);
    return 0;
}
