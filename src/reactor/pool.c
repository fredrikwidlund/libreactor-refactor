#include <stdlib.h>

#include "list.h"
#include "vector.h"
#include "pool.h"
#include "reactor.h"

#define POOL_CHUNK_SIZE 128

void pool_construct(pool *pool, size_t size)
{
  *pool = (struct pool) {.size = size};

  list_construct(&pool->chunks);
  vector_construct(&pool->free, sizeof(void *));
}

void pool_destruct(pool *pool)
{
  list_destruct(&pool->chunks, NULL);
  vector_destruct(&pool->free, NULL);
}

size_t pool_size(pool *pool)
{
  return pool->allocated;
}

void *pool_malloc(pool *pool)
{
  void *chunk, *object;
  size_t i;

  if (vector_size(&pool->free) == 0)
  {
    chunk = list_push_back(&pool->chunks, NULL, pool->size * POOL_CHUNK_SIZE);
    for (i = 0; i < POOL_CHUNK_SIZE; i++)
    {
      object = (char *) chunk + (i * pool->size);
      vector_push_back(&pool->free, &object);
    }
  }

  object = *(void **) vector_back(&pool->free);
  vector_pop_back(&pool->free, NULL);
  pool->allocated++;
  return object;
}

void pool_free(pool *pool, void *object)
{
  vector_push_back(&pool->free, &object);
  pool->allocated--;
}
