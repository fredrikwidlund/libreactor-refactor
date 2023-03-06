#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../reactor.h"

typedef struct object_header object_header;

struct object_header
{
  _Atomic size_t   ref;
  uint64_t         data;
  object_destroy  *destroy;
  char             object[];
};

void *object_create(void *base, size_t size, uint64_t data, object_destroy *destroy)
{
  object_header *header;

  header = malloc(sizeof *header + size);
  *header = (object_header) {.ref = 1, .data = data, .destroy = destroy};
  if (base)
    memcpy(header->object, base, size);
  return header->object;
}

void object_hold(void *object)
{
  object_header *header = (object_header *) ((char *) object - offsetof(object_header, object));

  header->ref++;
}

void object_release(void *object)
{
  object_header *header = (object_header *) ((char *) object - offsetof(object_header, object));

  if (__sync_sub_and_fetch(&header->ref, 1) == 0)
  {
    if (header->destroy)
      header->destroy(object);
    free(header);
  }
}

uint64_t object_data(void *object)
{
  object_header *header = (object_header *) ((char *) object - offsetof(object_header, object));

  return header->data;
}
