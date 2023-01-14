#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <reactor.h>

typedef struct data data;

struct data
{
  void   *base;
  size_t  size;
};

data data_define(void *base, size_t size)
{
  return (data) {.base = base, .size = size};
}

void *data_base(data data)
{
  return data.base;
}

size_t data_size(data data)
{
  return data.size;
}

#define STREAM_READ_SIZE (128 * 1024)

enum
{
  STREAM_ERROR,
  STREAM_CLOSE,
  STREAM_READ,
  STREAM_WRITE
};

typedef struct stream stream;

struct stream
{
  reactor_user  user;
  int           fd;

  size_t        read_offset;
  buffer        read;
  reactor_id    read_call;

  size_t        write_offset;
  buffer        write_current;
  buffer        write_next;
  reactor_id    write_call;
};

static void stream_cancel_callback(reactor_event *event)
{
  buffer *buffer = event->state;

  buffer_destruct(buffer);
  free(buffer);
}

static void stream_read_callback(reactor_event *event)
{
  stream *stream = event->state;
  int res = event->data;

  stream->read_call = 0;
  if (res > 0)
  {
    stream->read_offset += res;
    buffer_resize(&stream->read, buffer_size(&stream->read) + res);
    reactor_call(&stream->user, STREAM_READ, res);
  }
  else if (res == 0)
  {
    reactor_call(&stream->user, STREAM_CLOSE, 0);
  }
  else
  {
    reactor_call(&stream->user, STREAM_ERROR, -res);
  }
}

data stream_data(stream *stream)
{
  return data_define(buffer_base(&stream->read), buffer_size(&stream->read));
}

void stream_consume(stream *stream, size_t size)
{
  buffer_erase(&stream->read, 0, size);
}

void stream_clear(stream *stream)
{
  buffer_clear(&stream->read);
}

void stream_construct(stream *stream, reactor_callback *callback, void *state)
{
  *stream = (struct stream) {.user = reactor_user_define(callback, state), .fd = -1};
  buffer_construct(&stream->read);
  buffer_construct(&stream->write_current);
  buffer_construct(&stream->write_next);
}

bool stream_is_open(stream *stream)
{
  return stream->fd >= 0;
}

bool stream_is_flushed(stream *stream)
{
  return buffer_size(&stream->write_current) == 0 && buffer_size(&stream->write_next) == 0;
}

void stream_close(stream *stream)
{
  if (!stream_is_open(stream))
    return;
  (void) reactor_close(NULL, NULL, stream->fd);
  stream->fd = -1;
}

void stream_destruct(stream *stream)
{
  stream_close(stream);
  assert(!stream->read_call);
  assert(!stream->write_call);
  /*
  if (stream->read_call)
  {
    read = malloc(sizeof *read);
    *read = stream->read;
    buffer_construct(&stream->read);
    reactor_cancel(stream->read_call, stream_cancel_callback, read);
    stream->read_call = 0;
  }
  */
  buffer_destruct(&stream->read);
  buffer_destruct(&stream->write_current);
  buffer_destruct(&stream->write_next);
}

void stream_open(stream *stream, int fd)
{
  stream->fd = fd;
}

void stream_read(stream *stream, size_t read_size)
{
  size_t size = buffer_size(&stream->read);

  buffer_reserve(&stream->read, size + read_size);
  stream->read_call = reactor_read(stream_read_callback, stream, stream->fd, buffer_end(&stream->read), read_size, stream->read_offset);
}

void stream_flush(stream *stream);

static void stream_write_callback(reactor_event *event)
{
  stream *stream = event->state;
  int res = event->data;

  stream->write_call = 0;
  assert(buffer_size(&stream->write_current) == (size_t) res);
  stream->write_offset += res;
  buffer_clear(&stream->write_current);
  if (buffer_size(&stream->write_next))
    stream_flush(stream);
  else
    reactor_call(&stream->user, STREAM_WRITE, 0);
}

void stream_flush(stream *stream)
{
  buffer b;

  if (stream->write_call)
    return;
  if (buffer_size(&stream->write_next) == 0)
    return;
  b = stream->write_current;
  stream->write_current = stream->write_next;
  stream->write_next = b;
  stream->write_call = reactor_write(stream_write_callback, stream, stream->fd,
                                     buffer_base(&stream->write_current), buffer_size(&stream->write_current), stream->write_offset);
}

void stream_write(stream *stream, data data)
{
  memcpy(buffer_allocate(&stream->write_next, data_size(data)), data_base(data), data_size(data));
}

struct state
{
  stream input;
  stream output;
};

static void io(reactor_event *event)
{
  struct state *state = event->state;

  switch (event->type)
  {
  case STREAM_READ:
    stream_write(&state->output, stream_data(&state->input));
    stream_clear(&state->input);
    stream_flush(&state->output);
    stream_read(&state->input, 65536);
    break;
  case STREAM_WRITE:
    if (!stream_is_open(&state->input))
      stream_close(&state->output);
    break;
  case STREAM_CLOSE:
    stream_close(&state->input);
    if (stream_is_flushed(&state->output))
      stream_close(&state->output);
    break;
  }
}

int main()
{
  struct state state = {0};

  reactor_construct();
  stream_construct(&state.input, io, &state);
  stream_open(&state.input, 0);
  stream_read(&state.input, 65536);
  stream_construct(&state.output, io, &state);
  stream_open(&state.output, 1);
  reactor_loop();
  stream_destruct(&state.input);
  stream_destruct(&state.output);
  reactor_destruct();
}
