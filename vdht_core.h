#ifndef __VDHT_CORE_H__
#define __VDHT_CORE_H__

enum {
    BE_STR,
    BE_INT,
    BE_LIST,
    BE_DICT,
    BE_BUT
};

struct be_node;
struct be_dict {
    char* key;
    struct be_node* val;
};

struct be_node {
    int type;
    union {
        char* s;
        int32_t i;
        struct be_node **l;
        struct be_dict *d;
    }val;
};


struct be_node* be_alloc(int);
void be_free(struct be_node*);

struct be_node* be_decode(char*, int);
struct be_node* be_decode_str(char*);

struct be_node* be_create_dict(void);
struct be_node* be_create_list(void);
struct be_node* be_create_str (char*);
struct be_node* be_create_buf (void*, int);
struct be_node* be_create_int (int);

int be_add_keypair (struct be_node*, char*, struct be_node*);
int be_add_list    (struct be_node*, struct be_node*);

int be_encode      (struct be_node*, char*, int);

int be_unpack_int  (struct be_node*, int*);
int be_unpack_buf  (struct be_node*, void*, int);
struct be_node* be_find_node(struct be_node*, char*);

#endif

