#ifndef REACTOR_BUFFER_H_INCLUDED
#define REACTOR_BUFFER_H_INCLUDED

#include <stdlib.h>
#include <stdbool.h>

#include "../reactor.h"

typedef struct buffer buffer;
struct buffer
{
  void   *base;
  size_t  size;
  size_t  capacity;
};

/* constructor/destructor */
void    buffer_construct(buffer *);
void    buffer_destruct(buffer *);

/* capacity */
bool    buffer_empty(buffer *);
size_t  buffer_size(buffer *);
size_t  buffer_capacity(buffer *);
void    buffer_reserve(buffer *, size_t);
void    buffer_resize(buffer *, size_t);
void    buffer_compact(buffer *);
void   *buffer_allocate(buffer *, size_t);

/* modifiers */
void    buffer_insert(buffer *, size_t, void *, size_t);
void    buffer_insert_fill(buffer *, size_t, size_t, void *, size_t);
void    buffer_erase(buffer *, size_t, size_t);
void    buffer_clear(buffer *);

/* element access */
data    buffer_data(buffer *);
void   *buffer_base(buffer *);
void   *buffer_at(buffer *, size_t);
void   *buffer_end(buffer *);

#endif /* REACTOR_BUFFER_H_INCLUDED */
