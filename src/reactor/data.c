#include <stdlib.h>
#include <string.h>

#include "../reactor.h"

data data_define(const void *base, size_t size)
{
  return (data) {.base = base, .size = size};
}

const void *data_base(data data)
{
  return data.base;
}

size_t data_size(data data)
{
  return data.size;
}

data data_string(const char *string)
{
  return data_define(string, strlen(string));
}

data data_offset(data data, size_t size)
{
  return data_define((const char *) data_base(data) + size, data_size(data) - size);
}
