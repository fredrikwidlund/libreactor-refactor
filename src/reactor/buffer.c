#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "buffer.h"

static size_t buffer_roundup(size_t size)
{
  size--;
  size |= size >> 1;
  size |= size >> 2;
  size |= size >> 4;
  size |= size >> 8;
  size |= size >> 16;
  size |= size >> 32;
  size++;

  return size;
}

/* constructor/destructor */

void buffer_construct(buffer *b)
{
  *b = (buffer) {0};
}

void buffer_destruct(buffer *b)
{
  free(b->base);
  buffer_construct(b);
}

/* capacity */

size_t buffer_size(buffer *b)
{
  return b->size;
}

size_t buffer_capacity(buffer *b)
{
  return b->capacity;
}

void buffer_reserve(buffer *b, size_t capacity)
{
  if (capacity > b->capacity)
  {
    capacity = buffer_roundup(capacity);
    b->base = realloc(b->base, capacity);
    b->capacity = capacity;
  }
}

void buffer_resize(buffer *b, size_t size)
{
  if (size > buffer_capacity(b))
    buffer_reserve(b, size);
  b->size = size;
}

void buffer_compact(buffer *b)
{
  if (b->capacity > b->size)
  {
    b->base = realloc(b->base, b->size);
    b->capacity = b->size;
  }
}

void *buffer_allocate(buffer *b, size_t size)
{
  void *p;

  buffer_reserve(b, b->size + size);
  p = (uint8_t *) b->base + b->size;
  b->size += size;
  return p;
}

/* modifiers */

void buffer_insert(buffer *b, size_t position, void *base, size_t size)
{
  buffer_reserve(b, b->size + size);
  if (position < b->size)
    memmove((char *) b->base + position + size, (char *) b->base + position, b->size - position);
  memcpy((char *) b->base + position, base, size);
  b->size += size;
}

void buffer_insert_fill(buffer *b, size_t position, size_t count, void *base, size_t size)
{
  size_t i;

  buffer_reserve(b, b->size + (count * size));
  if (position < b->size)
    memmove((char *) b->base + position + (count * size), (char *) b->base + position, b->size - position);

  for (i = 0; i < count; i++)
    memcpy((char *) b->base + position + (i * size), base, size);
  b->size += count * size;
}

void buffer_erase(buffer *b, size_t position, size_t size)
{
  memmove((char *) b->base + position, (char *) b->base + position + size, b->size - position - size);
  b->size -= size;
}

void buffer_clear(buffer *b)
{
  b->size = 0;
}

/* element access */

void *buffer_base(buffer *b)
{
  return b->base;
}

void *buffer_at(buffer *b, size_t size)
{
  return (char *) b->base + size;
}

void *buffer_end(buffer *b)
{
  return buffer_at(b, b->size);
}
