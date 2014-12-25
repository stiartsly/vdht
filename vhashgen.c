#include "vglobal.h"
#include "vhashgen.h"

/*
 * the routine to hash system service information, such as stun, relay.
 * @gen:
 * @magic:
 * @hash:
 */
static
int _vhashgen_hash(struct vhashgen* gen, char* magic, vtoken* hash)
{
    vassert(gen);
    vassert(magic);
    vassert(hash);

    //todo;
    return 0;
}

/*
 * the routine to hash custom service information, such as popocloud service.
 * @gen:
 * @magic:
 * @cookie:
 * @hash
 */
static
int _vhashgen_hash_ext(struct vhashgen* gen, char* magic, char* cookie, vtoken* hash)
{
    vassert(gen);
    vassert(magic);
    vassert(cookie);
    vassert(hash);

    //todo;
    return 0;
}

/*
 * the routine to hash a framgent of content, such as content of file, or message.
 * @gen:
 * @content:
 * @len:
 * @hash: [out] hash vluae
 */
static
int _vhashgen_hash_frag(struct vhashgen* gen, char* content, int len, vtoken* hash)
{
    vassert(gen);
    vassert(content);
    vassert(len > 0);
    vassert(hash);

    //todo;
    return 0;
}

static
struct vhashgen_ops hashgen_ops = {
    .hash      = _vhashgen_hash,
    .hash_ext  = _vhashgen_hash_ext,
    .hash_frag = _vhashgen_hash_frag
};

int vhashgen_init  (struct vhashgen* gen)
{
    vassert(gen);

    gen->ops = &hashgen_ops;
    return 0;
}

void vhashgen_deinit(struct vhashgen* gen)
{
    vassert(gen);

    //todo;
    return ;
}

