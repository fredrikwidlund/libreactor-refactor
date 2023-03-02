#ifndef REACTOR_STREAM_H_INCLUDED
#define REACTOR_STREAM_H_INCLUDED

#include "../reactor.h"

enum stream_type
{
  STREAM_TYPE_UNDEFINED = 0,
  STREAM_TYPE_FILE,
  STREAM_TYPE_SOCKET
};

enum
{
  STREAM_ERROR,
  STREAM_CLOSE,
  STREAM_READ,
  STREAM_FLUSHED
};

typedef enum stream_type     stream_type;
typedef struct stream_input  stream_input;
typedef struct stream_output stream_output;
typedef struct stream        stream;

struct stream_input
{
  buffer         data;
  size_t         offset;
  reactor_id     id;
};

struct stream_output
{
  buffer         data;
  buffer         data_next;
  size_t         offset;
  size_t         partial_write;
  reactor_id     id;
};

struct stream
{
  reactor_user   user;
  int            fd;
  stream_type    type;
  stream_input   input;
  stream_output  output;
};

void stream_construct(stream *, reactor_callback *, void *);
void stream_destruct(stream *);
void stream_open(stream *, int, stream_type);
int  stream_fd(stream *);
// XXX add flag for not closing socket, and default to close?
void stream_close(stream *);
void stream_read(stream *, size_t);
void stream_consume(stream *, size_t);
void stream_write(stream *, data);
void stream_flush(stream *);
bool stream_flushed(stream *);
data stream_data(stream *);

#endif /* REACTOR_STREAM_H_INCLUDED */
