#ifndef __rdhash_h__
#define __rdhash_h__

#include <stdint.h>

typedef struct {
    void             *value;
    u_short           len;
    u_char            name[1];
} hash_elt_t;

typedef struct {
	size_t           klen;
	u_char          *kdata;
    uintptr_t        khash;
    void            *value;
} hash_key_t;

typedef struct {
	hash_key_t      *elts;
	size_t           size;
	size_t           cap;
} hash_array_t;

typedef struct {
	hash_elt_t      **buckets;
	size_t            size;
} hash_t;


hash_array_t *hash_array_init(size_t size);
int           hash_array_push(hash_array_t *st, u_char *key, void *value);

hash_t *hash_init(hash_array_t *st, unsigned int max_size);
void   *hash_find(hash_t *hash, u_char *name, size_t len);


#endif
