#ifndef REACTOR_HASH_H_INCLUDED
#define REACTOR_HASH_H_INCLUDED

#include <stdint.h>

uint64_t hash_string(char *);
uint64_t hash_data(void *, size_t);
uint64_t hash_uint64(uint64_t);

#endif /* REACTOR_HASH_H_INCLUDED */
