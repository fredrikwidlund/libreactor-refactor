#ifndef REACTOR_POOL_H
#define REACTOR_POOL_H

#include "../reactor.h"

typedef struct pool pool;

struct pool
{
  size_t size;
  size_t allocated;
  list   chunks;
  vector free;
};

void    pool_construct(pool *, size_t);
void    pool_destruct(pool *);
size_t  pool_size(pool *);
void   *pool_malloc(pool *);
void    pool_free(pool *, void *);

#endif /* REACTOR_POOL_H */
