#ifndef REACTOR_DATA_H_INCLUDED
#define REACTOR_DATA_H_INCLUDED

#include <stdlib.h>
#include <stdbool.h>

#include "../reactor.h"

typedef struct data data;

struct data
{
  const void *base;
  size_t      size;
};

data        data_define(const void *, size_t);
const void *data_base(data);
size_t      data_size(data);
bool        data_empty(data);
bool        data_equal(data, data);
data        data_null(void);
data        data_string(const char *);
data        data_offset(data, size_t);

#endif /* REACTOR_DATA_H_INCLUDED */
