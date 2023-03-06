#ifndef REACTOR_OBJECT_H_INCLUDED
#define REACTOR_OBJECT_H_INCLUDED

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "../reactor.h"

typedef void object_destroy(void *);

void     *object_create(void *, size_t, uint64_t, object_destroy *);
void      object_hold(void *);
void      object_release(void *);
uint64_t  object_data(void *);

#endif /* REACTOR_OBJECT_H_INCLUDED */
