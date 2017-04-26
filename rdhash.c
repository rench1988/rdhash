#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rdhash.h"

#define ha_cacheline_size  128

#define ha_hash(key, c)    ((uint_t) key * 31 + c)
#define ha_tolower(c)      (u_char) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c) 
//#define ha_calloc(x, n)    do { (x) = calloc((n), sizeof(x)); } while (0)
#define ha_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define ha_elt_size(name)                                               \
    (sizeof(void *) + ha_align((name)->klen + 2, sizeof(void *)))
#define ha_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

typedef unsigned int uint_t;

typedef struct {
    hash_t       *hash;

    uint_t        max_size;
    uint_t        bucket_size;
} hash_init_t;


static uint_t hash_key_lc(u_char *data, size_t len) {
    uint_t  i, key;

    key = 0;

    for (i = 0; i < len; i++) {
        key = ha_hash(key, ha_tolower(data[i]));
    }

    return key;
}

static int hash_array_enlarge(hash_array_t *st) {
	hash_key_t      *new;

	new = calloc(2 * st->size, sizeof(hash_key_t));
	if (new == NULL) {
		return -1;
	}

	memcpy(new, st->elts, sizeof(hash_key_t) * st->size);

	free(st->elts);

	st->elts = new;
	st->cap = 2 * st->size;

	return 0;
}

hash_array_t *hash_array_init(size_t size) {
	hash_array_t *at;

	if (!(at = calloc(1, sizeof(hash_array_t))) || !(at->elts = calloc(size, sizeof(hash_key_t)))) {
		return NULL;
	}

	at->size = 0;
	at->cap = size;

	return at;
}

int hash_array_push(hash_array_t *st, u_char *key, void *value) {
	if (st->size >= st->cap && hash_array_enlarge(st)) {
		return -1;
	}

	st->elts[st->size].kdata = key;
	st->elts[st->size].klen  = strlen(key);
	st->elts[st->size].khash = hash_key_lc(key, strlen(key));
	st->elts[st->size].value = value;

	st->size++;

	return 0;
}

static void hash_strlow(u_char *dst, u_char *src, size_t n) {
    while (n) {
        *dst = ha_tolower(*src);
        dst++;
        src++;
        n--;
    }
}

static uint_t hash_next_power_of_2(uint_t v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;

	return v;	
}

static uint_t hash_bucket_size(hash_key_t *keys, size_t size) {
	uint_t   n, max = 0;

	for (n = 0; n < size; n++) {
		if (max < ha_elt_size(&keys[n]) + sizeof(void *)) {
			max = ha_elt_size(&keys[n]) + sizeof(void *);
		}
	}

	return hash_next_power_of_2(max);
}

static int hash_init_helper(hash_init_t *hinit, hash_key_t *keys, size_t nelts) {
	size_t           len;
	u_char          *elts = NULL;
	uint_t           i, n, key, size, start, bucket_size;
	u_short         *test = NULL;
	hash_elt_t      *elt, **buckets = NULL;

	test = calloc(hinit->max_size, sizeof(u_short));
    if (test == NULL) {
        return -1;
    }

    bucket_size = hinit->bucket_size - sizeof(void *);

    start = nelts / (bucket_size / (2 * sizeof(void *)));
    start = start ? start : 1;

    if (hinit->max_size > 10000 && nelts && hinit->max_size / nelts < 100) {
        start = hinit->max_size - 1000;
    }

    for (size = start; size <= hinit->max_size; size++) {
    	memset(test, 0x00, size * sizeof(u_short));

    	for (n = 0; n < nelts; n++) {
            if (keys[n].kdata == NULL) {
                continue;
            }

            key = keys[n].khash % size;
            test[key] = (u_short) (test[key] + ha_elt_size(&keys[n]));

            if (test[key] > (u_short) bucket_size) {
                goto next;
            }
        }

        goto found;

    next:

        continue;                		
    }

    size = hinit->max_size;

found:

    for (i = 0; i < size; i++) {
        test[i] = sizeof(void *);
    }

    for (n = 0; n < nelts; n++) {
        if (keys[n].kdata == NULL) {
            continue;
        }

        key = keys[n].khash % size;
        test[key] = (u_short) (test[key] + ha_elt_size(&keys[n]));
    }

    len = 0;

    for (i = 0; i < size; i++) {
        if (test[i] == sizeof(void *)) {
            continue;
        }

        test[i] = (u_short) (ha_align(test[i], ha_cacheline_size));

        len += test[i];
    }

    buckets = calloc(size, sizeof(hash_elt_t *));
    if (buckets == NULL) {
    	goto failed;
    }

    elts = calloc(1, len + ha_cacheline_size);
    if (elts == NULL) {
        goto failed;
    }

    elts = ha_align_ptr(elts, ha_cacheline_size);

    for (i = 0; i < size; i++) {
        if (test[i] == sizeof(void *)) {
            continue;
        }

        buckets[i] = (hash_elt_t *) elts;
        elts += test[i];
    }

    for (i = 0; i < size; i++) {
        test[i] = 0;
    }

    for (n = 0; n < nelts; n++) {
        if (keys[n].kdata == NULL) {
            continue;
        }

        key = keys[n].khash % size;
        elt = (hash_elt_t *) ((u_char *) buckets[key] + test[key]);

        elt->value = keys[n].value;
        elt->len = (u_short) keys[n].klen;

        hash_strlow(elt->name, keys[n].kdata, keys[n].klen);

        test[key] = (u_short) (test[key] + ha_elt_size(&keys[n]));
    }

    for (i = 0; i < size; i++) {
        if (buckets[i] == NULL) {
            continue;
        }

        elt = (hash_elt_t *) ((u_char *) buckets[i] + test[i]);

        elt->value = NULL;
    }

    free(test);

    hinit->hash->buckets = buckets;
    hinit->hash->size = size;      

    return 0;

failed:
    if (test) free(test);
    if (buckets) free(buckets);
    if (elts) free(elts);

    return -1;
}

hash_t *hash_init(hash_array_t *st, uint_t max_size) {
	hash_init_t      calls_hash;
	hash_t          *hash;

	hash = calloc(1, sizeof(hash_t));
	if (hash == NULL) {
		return NULL;
	}

	calls_hash.hash        = hash;
    calls_hash.max_size    = max_size;
    calls_hash.bucket_size = hash_bucket_size(st->elts, st->size);

    if (hash_init_helper(&calls_hash, st->elts, st->size)) {
    	free(hash);
        return NULL;
    }    

    return hash;
}

void  *hash_find(hash_t *hash, u_char *name, size_t len) {
	uint_t           key, i;
	hash_elt_t      *elt;

	key = hash_key_lc(name, len);

	elt = hash->buckets[key % hash->size];

	if (elt == NULL) {
        return NULL;
    }

    while (elt->value) {
        if (len != (size_t) elt->len) {
            goto next;
        }

        for (i = 0; i < len; i++) {
            if (name[i] != elt->name[i]) {
                goto next;
            }
        }

        return elt->value;

    next:

        elt = (hash_elt_t *) ha_align_ptr(&elt->name[0] + elt->len,
                                               sizeof(void *));
        continue;
    }

    return NULL;
}

