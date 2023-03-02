#ifndef REACTOR_HTTP_H_INCLUDED
#define REACTOR_HTTP_H_INCLUDED

#include "data.h"
#include "buffer.h"

typedef struct http_field http_field;

struct http_field
{
  data name;
  data value;
};

int  http_read_request(data, data *, data *, http_field *, size_t *);
void http_write_response(buffer *, data, data, data, data);

#endif /* REACTOR_HTTP_H_INCLUDED */
