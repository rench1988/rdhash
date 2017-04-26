#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "rdhash.h"

#define elem_num  10000

typedef struct {
    int a;
} test;

void TEST_F() {
	int            i;
	hash_array_t  *ha;
	test          *t, *te;
	u_char        *key;

	ha = hash_array_init(elem_num / 2);

	for (i = 0; i < elem_num; i++) {
		key = malloc(6);

		t = malloc(sizeof(test));

		snprintf(key, 6, "%d", i);
		t->a = i;

		hash_array_push(ha, key, t);
	}

	hash_t *hash = hash_init(ha, 256);

	char buf[6];
	for (i = 0; i < elem_num; i++) {
		snprintf(buf, sizeof(buf), "%d", i);
		te = hash_find(hash, buf, strlen(buf));

		assert(te->a == i);
	}
}

int main(int argc, char const *argv[]) {

	TEST_F();

	printf("passed!\n");

	return 0;
}

