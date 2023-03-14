#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>

#include "../reactor.h"

static void stream_send(stream *);

static void stream_recv_callback(reactor_event *event)
{
  stream *stream = event->state;
  int result = event->data;
  data data;

  stream->input.id = 0;

  if (result == 0)
  {
    reactor_call(&stream->user, STREAM_CLOSE, 0);
    return;
  }

  if (result < 0)
  {
    reactor_call(&stream->user, STREAM_ERROR, -result);
    return;
  }

  stream->input.offset += result;
  data = data_define(buffer_end(&stream->input.data), result);
  buffer_resize(&stream->input.data, buffer_size(&stream->input.data) + result);
  reactor_call(&stream->user, STREAM_READ, (uint64_t) &data);
}

static void stream_send_callback(reactor_event *event)
{
  stream *stream = event->state;
  int result = event->data;

  stream->output.id = 0;

  if (result <= 0)
  {
    reactor_call(&stream->user, STREAM_ERROR, -result);
    return;
  }

  /* partial write, continue writing */
  if (buffer_size(&stream->output.data) - stream->output.partial_write != (size_t) result)
  {
    stream->output.partial_write += result;
    stream_send(stream);
    return;
  }

  stream->output.partial_write = 0;
  buffer_clear(&stream->output.data);

  stream->output.offset += result;
  stream_flush(stream);
  if (stream_flushed(stream))
    reactor_call(&stream->user, STREAM_FLUSHED, 0);
}

static void stream_send(stream *stream)
{
  if (stream->type == STREAM_TYPE_SOCKET)
    stream->output.id = reactor_send(stream_send_callback, stream, stream->fd,
                                     (char *) buffer_base(&stream->output.data) + stream->output.partial_write,
                                     buffer_size(&stream->output.data) - stream->output.partial_write, 0);
  else
    stream->output.id = reactor_write(stream_send_callback, stream, stream->fd,
                                      buffer_base(&stream->output.data), buffer_size(&stream->output.data), stream->output.offset);
}

void stream_construct(stream *stream, reactor_callback *callback, void *state)
{
  *stream = (struct stream) {.user = reactor_user_define(callback, state), .fd = -1};

  buffer_construct(&stream->input.data);
  buffer_construct(&stream->output.data);
  buffer_construct(&stream->output.data_next);
}

void stream_destruct(stream *stream)
{
  stream_close(stream);
  buffer_destruct(&stream->input.data);
  buffer_destruct(&stream->output.data);
  buffer_destruct(&stream->output.data_next);
}

void stream_open(stream *stream, int fd, stream_type type)
{
  struct stat st;
  int e;

  stream->fd = fd;
  stream->type = type;

  if (type == STREAM_TYPE_UNDEFINED)
  {
    e = fstat(stream->fd, &st);
    assert(e == 0);
    stream->type = S_ISSOCK(st.st_mode) ? STREAM_TYPE_SOCKET : STREAM_TYPE_FILE;
  }
}

int stream_fd(stream *stream)
{
  return stream->fd;
}

static void stream_release_buffer(reactor_event *event)
{
  free(event->state);
}

static void stream_cancel_io(reactor_id id, buffer *buffer)
{
  reactor_cancel(id, stream_release_buffer, buffer_deconstruct(buffer));
}

void stream_close(stream *stream)
{
  if (stream->input.id)
  {
    stream_cancel_io(stream->input.id, &stream->input.data);
    stream->input.id = 0;
  }
  if (stream->output.id)
  {
    stream_cancel_io(stream->output.id, &stream->output.data);
    stream->output.id = 0;
  }
  if (stream->fd >= 0)
    stream->fd = -1;
}

void stream_read(stream *stream, size_t size)
{
  buffer_reserve(&stream->input.data, buffer_size(&stream->input.data) + size);
  assert(stream->input.id == 0);
  if (stream->type == STREAM_TYPE_SOCKET)
    stream->input.id = reactor_recv(stream_recv_callback, stream, stream->fd, buffer_end(&stream->input.data), size, 0);
  else
    stream->input.id = reactor_read(stream_recv_callback, stream, stream->fd, buffer_end(&stream->input.data), size, stream->input.offset);
}

void stream_consume(stream *stream, size_t size)
{
  buffer_erase(&stream->input.data, 0, size);
}

void stream_write(stream *stream, data data)
{
  memcpy(buffer_allocate(&stream->output.data_next, data_size(data)), data_base(data), data_size(data));
}

void stream_flush(stream *stream)
{
  buffer buffer;

  if (stream_flushed(stream))
    return;
  if (stream->output.id)
    return;
  buffer = stream->output.data;
  stream->output.data = stream->output.data_next;
  stream->output.data_next = buffer;
  stream_send(stream);
}

bool stream_flushed(stream *stream)
{
  return buffer_empty(&stream->output.data) && buffer_empty(&stream->output.data_next);
}

data stream_data(stream *stream)
{
  return buffer_data(&stream->input.data);
}
