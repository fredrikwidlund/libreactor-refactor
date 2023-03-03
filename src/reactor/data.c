#include <stdlib.h>
#include <stdbool.h>
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

bool data_empty(data data)
{
  return data_size(data) == 0;
}

bool data_equal(data data1, data data2)
{
  return data_size(data1) == data_size(data2) && memcmp(data_base(data1), data_base(data2), data_size(data1)) == 0;
}

bool data_equal_case(data data1, data data2)
{
  return data_size(data1) == data_size(data2) && strncasecmp(data_base(data1), data_base(data2), data_size(data1)) == 0;
}

data data_string(const char *string)
{
  return data_define(string, strlen(string));
}

data data_null(void)
{
  return data_define(NULL, 0);
}

data data_offset(data data, size_t size)
{
  return data_define((const char *) data_base(data) + size, data_size(data) - size);
}
