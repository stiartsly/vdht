#include "vglobal.h"
#include "vhashgen.h"

#define vsha1_circular_shift(bits,word) ((((word) << (bits)) & 0xFFFFFFFF) | ((word) >> (32-(bits))))
static
void _aux_sha1_compute_msgblk(struct vhashgen_ctxt_sha1* ctxt)
{
    uint32_t k[] = /* Constants defined in SHA-1   */
    {
        0x5A827999,
        0x6ED9EBA1,
        0x8F1BBCDC,
        0xCA62C1D6
    };
    int i = 0;
    uint32_t tmp;
    uint32_t seq[80];
    uint32_t a,b,c,d,e;

    vassert(ctxt);

    for (i = 0; i < 16; i++) {
        seq[i]  = ((uint32_t)ctxt->msgblk[i*4]) << 24;
        seq[i] |= ((uint32_t)ctxt->msgblk[i*4 + 1]) << 16;
        seq[i] |= ((uint32_t)ctxt->msgblk[i*4 + 2]) << 8;
        seq[i] |= ((uint32_t)ctxt->msgblk[i*4 + 3]);
    }

    for (i = 16; i < 80; i++) {
        seq[i] = vsha1_circular_shift(1, seq[i-3] ^ seq[i-14] ^ seq[i-16]);
    }
    a = ctxt->digest[0];
    b = ctxt->digest[1];
    c = ctxt->digest[2];
    d = ctxt->digest[3];
    e = ctxt->digest[4];

    for (i = 0; i < 20; i++) {
        tmp = vsha1_circular_shift(5, a) + ((b & c) | ((~b) & d)) + e + seq[i] + k[0];
        tmp &= 0xFFFFFFFF;
        e = d;
        d = c;
        c = vsha1_circular_shift(30, b);
        b = a;
        a = tmp;
    }
    for (i = 20; i < 40; i++) {
        tmp = vsha1_circular_shift(5, a) + (b ^ c ^ d) + e + seq[i] + k[1];
        tmp &= 0xFFFFFFFF;
        e = d;
        d = c;
        c = vsha1_circular_shift(30, b);
        b = a;
        a = tmp;
    }
    for ( i = 40; i < 60; i++){
        tmp = vsha1_circular_shift(5, a) + ((b & c) | (b & c) | (c & d)) + e + seq[i] + k[2];
        tmp &= 0xFFFFFFFF;
        e = d;
        d = c;
        c = vsha1_circular_shift(30, b);
        b = a;
        a = tmp;
    }
    for ( i = 60; i < 80; i++) {
        tmp = vsha1_circular_shift(5, a) +  (b ^ c ^ d) + e + seq[i] + k[3];
        tmp &= 0xFFFFFFFF;
        e = d;
        d = c;
        c = vsha1_circular_shift(30, b);
        b = a;
        a = tmp;
    }

    ctxt->digest[0] = (ctxt->digest[0] + a) & 0xFFFFFFFF;
    ctxt->digest[1] = (ctxt->digest[1] + b) & 0xFFFFFFFF;
    ctxt->digest[2] = (ctxt->digest[2] + c) & 0xFFFFFFFF;
    ctxt->digest[3] = (ctxt->digest[3] + d) & 0xFFFFFFFF;
    ctxt->digest[4] = (ctxt->digest[4] + e) & 0xFFFFFFFF;

    ctxt->msgblk_idx = 0;
    return ;
}

static
void _aux_sha1_pad_msgblk(struct vhashgen_ctxt_sha1* ctxt)
{
    vassert(ctxt);
    /*
     *  Check to see if the current message block is too small to hold
     *  the initial padding bits and length.  If so, we will pad the
     *  block, process it, and then continue padding into a second
     *  block.
     */
    if (ctxt->msgblk_idx > 55) {
        ctxt->msgblk[ctxt->msgblk_idx++] = 0x80;
        while (ctxt->msgblk_idx < 64) {
            ctxt->msgblk[ctxt->msgblk_idx++] = 0;
        }
        _aux_sha1_compute_msgblk(ctxt);
        while (ctxt->msgblk_idx < 56) {
            ctxt->msgblk[ctxt->msgblk_idx++] = 0;
        }
    } else {
        ctxt->msgblk[ctxt->msgblk_idx++] = 0x80;
        while (ctxt->msgblk_idx <  56) {
            ctxt->msgblk[ctxt->msgblk_idx++] = 0;
        }
    }

    /*
     *  Store the message length as the last 8 octets
     */
    ctxt->msgblk[56] = (ctxt->len_high >> 24) & 0xFF;
    ctxt->msgblk[57] = (ctxt->len_high >> 16) & 0xFF;
    ctxt->msgblk[58] = (ctxt->len_high >> 8 ) & 0xFF;
    ctxt->msgblk[59] = (ctxt->len_high) & 0xFF;
    ctxt->msgblk[60] = (ctxt->len_low >> 24 ) & 0xFF;
    ctxt->msgblk[61] = (ctxt->len_low >> 16 ) & 0xFF;
    ctxt->msgblk[62] = (ctxt->len_low >> 8  ) & 0xFF;
    ctxt->msgblk[63] = (ctxt->len_low) & 0xFF;

    _aux_sha1_compute_msgblk(ctxt);
    return ;
}

static
void _vhashgen_sha1_reset(struct vhashgen* gen)
{
    struct vhashgen_ctxt_sha1* ctxt = &gen->ctxt_sha1;
    vassert(gen);

    ctxt->len_low  = 0;
    ctxt->len_high = 0;

    ctxt->digest[0] = 0x67452301;
    ctxt->digest[1] = 0xEFCDAB89;
    ctxt->digest[2] = 0x98BADCFE;
    ctxt->digest[3] = 0x10325476;
    ctxt->digest[4] = 0xC3D2E1F0;

    ctxt->done = 0;
    ctxt->oops = 0;

    return ;
}

static
void _vhashgen_sha1_input(struct vhashgen* gen, uint8_t* msg, int len)
{
    struct vhashgen_ctxt_sha1* ctxt = &gen->ctxt_sha1;
    vassert(gen);
    vassert(msg);
    vassert(len >= 0);

    retE_v((!len));
    if (ctxt->done || ctxt->oops) {
        ctxt->oops = 1;
        return ;
    }
    while (len-- && !ctxt->oops) {
        ctxt->msgblk[ctxt->msgblk_idx++] = (*msg & 0xff);
        ctxt->len_low += 8;
        ctxt->len_low &= 0xFFFFFFFF;
        if (!ctxt->len_low) {
            ctxt->len_high++;
            ctxt->len_high &=  0xFFFFFFFF;
            if (!ctxt->len_high) {
                ctxt->oops = 1;
            }
        }
        if (ctxt->msgblk_idx == 64) {
            _aux_sha1_compute_msgblk(ctxt);
        }
        msg++;
    }
    return ;
}

static
int _vhashgen_sha1_result(struct vhashgen* gen, uint32_t* hash_array)
{
    struct vhashgen_ctxt_sha1* ctxt = &gen->ctxt_sha1;
    vassert(gen);
    vassert(hash_array);

    retE((ctxt->oops));
    if ((!ctxt->done)) {
        _aux_sha1_pad_msgblk(ctxt);
        ctxt->done = 1;
    }

    hash_array[0] = ctxt->digest[0];
    hash_array[1] = ctxt->digest[1];
    hash_array[2] = ctxt->digest[2];
    hash_array[3] = ctxt->digest[3];
    hash_array[4] = ctxt->digest[4];

    return 0;
}

static
struct vhashgen_sha1_ops hashgen_sha1_ops = {
    .reset  = _vhashgen_sha1_reset,
    .input  = _vhashgen_sha1_input,
    .result = _vhashgen_sha1_result,
};

/*
 * the routine to hash system service information, such as stun, relay.
 * @gen:
 * @magic:
 * @hash:
 */
static
int _vhashgen_hash(struct vhashgen* gen, uint8_t* magic, vtoken* hash)
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
int _vhashgen_hash_ext(struct vhashgen* gen, uint8_t* magic, uint8_t* cookie, vtoken* hash)
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
int _vhashgen_hash_frag(struct vhashgen* gen, uint8_t* content, int len, vtoken* hash)
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

int vhashgen_init (struct vhashgen* gen)
{
    vassert(gen);

    memset(&gen->ctxt_sha1, 0, sizeof(struct vhashgen_ctxt_sha1));

    gen->ops     = &hashgen_ops;
    gen->sha_ops = &hashgen_sha1_ops;

    return 0;
}

void vhashgen_deinit(struct vhashgen* gen)
{
    vassert(gen);

    //todo;
    return ;
}

