#include "vdhtapi.h"

__thread int verrno = 0;

enum {
    vsucc = 0,
    verr_bad_args,
};

int vdhtc_hash(uint8_t* msg, int len, vsrvcHash* hash)
{
    if (!msg || len <= 0) {
        verrno = verr_bad_args;
        return -1;
    }
    if (!hash) {
        verrno = verr_bad_args;
        return -1;
    }

    //todo;
    return 0;
}

int vdhtc_hash_with_cookie(uint8_t* msg, int len, uint8_t* cookie, int cookie_len, vsrvcHash* hash)
{
    if (!msg || len <= 0) {
        verrno = verr_bad_args;
        return -1;
    }
    if (!cookie || cookie_len <= 0) {
        verrno = verr_bad_args;
        return -1;
    }
    if (!hash) {
        verrno = verr_bad_args;
        return -1;
    }

    //todo;
    return 0;
}

int vdhtc_post_service_segment  (vsrvcHash* srvcHash, struct sockaddr_in* addr)
{
    if (!srvcHash || !addr) {
        verrno = verr_bad_args;
        return -1;
    }

    //todo;
    return 0;
}

int vdhtc_unpost_service_segment(vsrvcHash* srvcHash, struct sockaddr_in* addr)
{
    if (!srvcHash || !addr) {
        verrno = verr_bad_args;
        return -1;
    }

    //todo;
    return 0;
}

int vdhtc_post_service(vsrvcHash* srvcHash, struct sockaddr_in* addrs, int num)
{
    if (!srvcHash) {
        verrno = verr_bad_args;
        return -1;
    }
    if (!addrs || num <= 0) {
        verrno = verr_bad_args;
        return -1;
    }

    //todo;
    return 0;
}

int vdhtc_unpost_service(vsrvcHash* srvcHash)
{
    if (!srvcHash) {
        verrno = verr_bad_args;
        return -1;
    }

    //todo;
    return 0;
}

int vdhtc_find_service(vsrvcHash* srvcHash, vsrvcInfo_iterate_addr_t cb, void* cookie)
{
    if (!srvcHash || !cb) {
        verrno = verr_bad_args;
        return -1;
    }

    //todo;
    return 0;
}

int vdhtc_probe_service(vsrvcHash* srvcHash, vsrvcInfo_iterate_addr_t cb, void* cookie)
{
    if (!srvcHash || !cb) {
        verrno = verr_bad_args;
        return -1;
    }

    //todo;
    return 0;
}

