#include "vglobal.h"

int main(int argc, char** argv)
{
    struct vhost* host = NULL;

    host = vhost_create("10.0.0.14", 12300);
    retE((!host));

    host->ops->loop(host);

    vhost_destroy(host);
    return 0;
}
