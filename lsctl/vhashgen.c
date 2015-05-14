#include <stdlib.h>
#include <string.h>
#include "vhashgen.h"

/*  Description:
 *      This file implements the Secure Hashing Standard as defined
 *      in FIPS PUB 180-1 published April 17, 1995.
 *
 *      The Secure Hashing Standard, which uses the Secure Hashing
 *      Algorithm (SHA), produces a 160-bit message digest for a
 *      given data stream.  In theory, it is highly improbable that
 *      two messages will produce the same message digest.  Therefore,
 *      this algorithm can serve as a means of providing a "fingerprint"
 *      for a message.
 *
 *  Portability Issues:
 *      SHA-1 is defined in terms of 32-bit "words".  This code was
 *      written with the expectation that the processor has at least
 *      a 32-bit machine word size.  If the machine word size is larger,
 *      the code should still function properly.  One caveat to that
 *      is that the input functions taking characters and character
 *      arrays assume that only 8 bits of information are stored in each
 *      character.
 *
 *  Caveats:
 *      SHA-1 is designed to work with messages less than 2^64 bits
 *      long. Although SHA-1 allows a message digest to be generated for
 *      messages of any number of bits less than 2^64, this
 *      implementation only works with messages with a length that is a
 *      multiple of the size of an 8-bit character.
 *
 */

struct vhashgen_ctxt_sha1 {
    uint32_t digest[5];
    int len_low;
    int len_high;       // msg len in bits.

    uint8_t msgbuf[64]; // 512-bit msgbuf
    int cursor;         // index to msgbuf

    int oops;
    int done;
};

static
void _aux_sha1_digest_msgbuf(struct vhashgen_ctxt_sha1* ctxt)
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

#define sha1_circular_shift(bits,word) ((((word) << (bits)) & 0xFFFFFFFF) | ((word) >> (32-(bits))))

    vassert(ctxt);

    for (i = 0; i < 16; i++) {
        seq[i]  = ((uint32_t)ctxt->msgbuf[i*4]) << 24;
        seq[i] |= ((uint32_t)ctxt->msgbuf[i*4 + 1]) << 16;
        seq[i] |= ((uint32_t)ctxt->msgbuf[i*4 + 2]) << 8;
        seq[i] |= ((uint32_t)ctxt->msgbuf[i*4 + 3]);
    }

    for (i = 16; i < 80; i++) {
        seq[i] = sha1_circular_shift(1, seq[i-3] ^ seq[i-14] ^ seq[i-16]);
    }
    a = ctxt->digest[0];
    b = ctxt->digest[1];
    c = ctxt->digest[2];
    d = ctxt->digest[3];
    e = ctxt->digest[4];

    for (i = 0; i < 20; i++) {
        tmp = sha1_circular_shift(5, a) + ((b & c) | ((~b) & d)) + e + seq[i] + k[0];
        tmp &= 0xFFFFFFFF;
        e = d;
        d = c;
        c = sha1_circular_shift(30, b);
        b = a;
        a = tmp;
    }
    for (i = 20; i < 40; i++) {
        tmp = sha1_circular_shift(5, a) + (b ^ c ^ d) + e + seq[i] + k[1];
        tmp &= 0xFFFFFFFF;
        e = d;
        d = c;
        c = sha1_circular_shift(30, b);
        b = a;
        a = tmp;
    }
    for ( i = 40; i < 60; i++){
        tmp = sha1_circular_shift(5, a) + ((b & c) | (b & c) | (c & d)) + e + seq[i] + k[2];
        tmp &= 0xFFFFFFFF;
        e = d;
        d = c;
        c = sha1_circular_shift(30, b);
        b = a;
        a = tmp;
    }
    for ( i = 60; i < 80; i++) {
        tmp = sha1_circular_shift(5, a) +  (b ^ c ^ d) + e + seq[i] + k[3];
        tmp &= 0xFFFFFFFF;
        e = d;
        d = c;
        c = sha1_circular_shift(30, b);
        b = a;
        a = tmp;
    }

    ctxt->digest[0] = (ctxt->digest[0] + a) & 0xFFFFFFFF;
    ctxt->digest[1] = (ctxt->digest[1] + b) & 0xFFFFFFFF;
    ctxt->digest[2] = (ctxt->digest[2] + c) & 0xFFFFFFFF;
    ctxt->digest[3] = (ctxt->digest[3] + d) & 0xFFFFFFFF;
    ctxt->digest[4] = (ctxt->digest[4] + e) & 0xFFFFFFFF;

    ctxt->cursor = 0;
    return ;
}

static
void _aux_sha1_padding_msgbuf(struct vhashgen_ctxt_sha1* ctxt)
{
   // vassert(ctxt);
    /*
     *  Check to see if the current message block is too small to hold
     *  the initial padding bits and length.  If so, we will pad the
     *  block, process it, and then continue padding into a second
     *  block.
     */
    if (ctxt->cursor > 55) {
        ctxt->msgbuf[ctxt->cursor++] = 0x80;
        while (ctxt->cursor < 64) {
            ctxt->msgbuf[ctxt->cursor++] = 0;
        }
        _aux_sha1_digest_msgbuf(ctxt);
        while (ctxt->cursor < 56) {
            ctxt->msgbuf[ctxt->cursor++] = 0;
        }
    } else {
        ctxt->msgbuf[ctxt->cursor++] = 0x80;
        while (ctxt->cursor <  56) {
            ctxt->msgbuf[ctxt->cursor++] = 0;
        }
    }

    /*
     *  Store the message length as the last 8 octets
     */
    ctxt->msgbuf[56] = (ctxt->len_high >> 24) & 0xFF;
    ctxt->msgbuf[57] = (ctxt->len_high >> 16) & 0xFF;
    ctxt->msgbuf[58] = (ctxt->len_high >> 8 ) & 0xFF;
    ctxt->msgbuf[59] = (ctxt->len_high) & 0xFF;
    ctxt->msgbuf[60] = (ctxt->len_low >> 24 ) & 0xFF;
    ctxt->msgbuf[61] = (ctxt->len_low >> 16 ) & 0xFF;
    ctxt->msgbuf[62] = (ctxt->len_low >> 8  ) & 0xFF;
    ctxt->msgbuf[63] = (ctxt->len_low) & 0xFF;

    _aux_sha1_digest_msgbuf(ctxt);
    return ;
}

/*
 * the routine to reset the context of sha1-hash engine.
 *
 * @gen: hash engine.
 */
static
void _vhashgen_sha1_reset(struct vhashgen* gen)
{
    struct vhashgen_ctxt_sha1* ctxt = (struct vhashgen_ctxt_sha1*)gen->ctxt;
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

/*
 * the routine to process the input message with sha1-hash algorithm.
 * @gen: hash engine.
 * @msg: message buf.
 * @len: message length
 */
static
void _vhashgen_sha1_input(struct vhashgen* gen, uint8_t* msg, int len)
{
    struct vhashgen_ctxt_sha1* ctxt = (struct vhashgen_ctxt_sha1*)gen->ctxt;
    vassert(gen);
    vassert(msg);
    vassert(len > 0);

    if (len <= 0) {
        ctxt->oops = 1;
        return ;
    }
    if (ctxt->done || ctxt->oops) {
        ctxt->oops = 1;
        return ;
    }
    while (len-- && !ctxt->oops) {
        ctxt->msgbuf[ctxt->cursor++] = (*msg & 0xff);
        ctxt->len_low += 8;
        ctxt->len_low &= 0xFFFFFFFF;
        if (!ctxt->len_low) {
            ctxt->len_high++;
            ctxt->len_high &=  0xFFFFFFFF;
            if (!ctxt->len_high) {
                ctxt->oops = 1;
            }
        }
        if (ctxt->cursor == 64) {
            _aux_sha1_digest_msgbuf(ctxt);
        }
        msg++;
    }
    return ;
}

/*
 * the routine to get hash result after processing.
 * @ gen:
 * @ hash_array: [out] hash result value;
 */
static
int _vhashgen_sha1_result(struct vhashgen* gen, uint32_t* hash_array)
{
    struct vhashgen_ctxt_sha1* ctxt = (struct vhashgen_ctxt_sha1*)gen->ctxt;
    vassert(gen);
    vassert(hash_array);

    if (ctxt->oops) {
        return -1;
    }
    if ((!ctxt->done)) {
        _aux_sha1_padding_msgbuf(ctxt);
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
 * the routine to hash a certain length of message, which often used to
 * hash system sort of services, such as relay, stun.
 *
 * @gen:
 * @content:
 * @len:
 * @hash:
 */
static
int _vhashgen_hash(struct vhashgen* gen, uint8_t* msg, int len, vsrvcHash* hash)
{
    int ret = 0;
    vassert(gen);
    vassert(msg);
    vassert(len > 0);
    vassert(hash);

    gen->sha_ops->reset(gen);
    gen->sha_ops->input(gen, msg, len);
    ret = gen->sha_ops->result(gen, (uint32_t*)hash->data);
    return ret;
}

/*
 * the routine to hash a certain length of message and cookie, which often
 * used to hash user-customized sort of services, such as popocloud, vpn.
 *
 * @gen:
 * @magic:
 * @cookie:
 * @hash
 */
static
int _vhashgen_hash_with_cookie(struct vhashgen* gen, uint8_t* content, int len, uint8_t* cookie, int cookie_len, vsrvcHash* hash)
{
    int ret = 0;
    vassert(gen);
    vassert(content);
    vassert(len > 0);
    vassert(cookie);
    vassert(cookie_len > 0);
    vassert(hash);

    gen->sha_ops->reset(gen);
    gen->sha_ops->input(gen, content, len);
    gen->sha_ops->input(gen, cookie, cookie_len);
    ret = gen->sha_ops->result(gen, (uint32_t*)hash->data);
    return ret;
}

static
struct vhashgen_ops hashgen_ops = {
    .hash             = _vhashgen_hash,
    .hash_with_cookie = _vhashgen_hash_with_cookie
};

int vhashgen_init (struct vhashgen* gen)
{
    void* ctxt = NULL;
    vassert(gen);

    ctxt = malloc(sizeof(struct vhashgen_ctxt_sha1));
    if (!ctxt) {
        return -1;
    }
    memset(ctxt, 0, sizeof(struct vhashgen_ctxt_sha1));

    gen->ctxt    = ctxt;
    gen->sha_ops = &hashgen_sha1_ops;
    gen->ops     = &hashgen_ops;

    return 0;
}

void vhashgen_deinit(struct vhashgen* gen)
{
    vassert(gen);
    if (gen->ctxt) {
        free(gen->ctxt);
    }
    return ;
}

