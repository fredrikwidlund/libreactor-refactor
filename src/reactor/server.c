#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "../reactor.h"

static void server_accept_next(server *);

static void server_date_update(server *server)
{
  time_t t;
  struct tm tm;
  static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  t = (reactor_now() + 10000000) / 1000000000;
  (void) gmtime_r(&t, &tm);
  (void) strftime(server->date_string, sizeof server->date_string, "---, %d --- %Y %H:%M:%S GMT", &tm);
  memcpy(server->date_string, days[tm.tm_wday], 3);
  memcpy(server->date_string + 8, months[tm.tm_mon], 3);
  server->date = data_define(server->date_string, 29);
}

static void server_flush(server_request *request)
{
  stream_flush(&request->stream);
}

static void server_read(server_request *request)
{
  stream_consume(&request->stream, request->stream_offset);
  request->stream_offset = 0;
  stream_read(&request->stream, 16384);
}

static void server_io(server_request *request)
{
  if (!stream_flushed(&request->stream))
    server_flush(request);
  else
    server_read(request);
}

static void server_parse(server_request *request)
{

  data input;
  int n;
  bool abort;

  input = data_offset(stream_data(&request->stream), request->stream_offset);
  request->fields_count = sizeof request->fields / sizeof request->fields[0];
  n = http_read_request(input, &request->method, &request->target, request->fields, &request->fields_count, &request->body);
  // XXX increase number of fields of -1 and count == max?
  if (n == -1)
    server_disconnect(request);
  else if (n == -2)
    server_io(request);
  else
  {
    assert(n > 0);
    request->stream_offset += n;
    abort = false;
    request->abort = &abort;
    request->in_progress = true;
    reactor_call(&request->server->user, SERVER_REQUEST, (uint64_t) request);
    if (abort)
      return;
    request->abort = NULL;
    if (request->in_progress)
      server_flush(request);
    else
      server_parse(request);
  }
}

static void server_stream_callback(reactor_event *event)
{
  server_request *request = event->state;

  switch (event->type)
  {
  case STREAM_READ:
    server_parse(request);
    break;
  case STREAM_FLUSHED:
    if (request->in_progress)
      break;
    if (data_size(stream_data(&request->stream)) == request->stream_offset)
      server_read(request);
    else
      server_parse(request);
    break;
  default:
    server_disconnect(request);
    break;
  }
}

static void server_accept_callback(reactor_event *event)
{
  server *server = event->state;
  int fd = event->data;

  server->accept = 0;
  server_accept(server, fd);
  server_accept_next(server);
}

static void server_timer_callback(reactor_event *event)
{
  server *server = event->state;

  server_date_update(server);
}

static void server_accept_next(server *server)
{
  assert(server->accept == 0);
  server->accept = reactor_accept(server_accept_callback, server, server->fd, NULL, NULL, 0);
}

void server_construct(server *server, reactor_callback *callback, void *state)
{
  *server = (struct server) {.user = reactor_user_define(callback, state), .fd = -1};
  list_construct(&server->requests);
  timer_construct(&server->timer, server_timer_callback, server);
}

void server_destruct(server *server)
{
  server_close(server);
  timer_destruct(&server->timer);
  list_destruct(&server->requests, NULL);
}

void server_open(server *server, int fd)
{
  server->fd = fd;
  timer_set(&server->timer, ((reactor_now() / 1000000000) * 1000000000), 1000000000);
  server_date_update(server);
  server_accept_next(server);
}

void server_accept(server *server, int fd)
{
  server_request *request;

  if (fd < 0)
    return;
  request = list_push_back(&server->requests, NULL, sizeof *request);
  *request = (server_request) {.server = server};
  stream_construct(&request->stream, server_stream_callback, request);
  stream_open(&request->stream, fd, STREAM_TYPE_SOCKET);
  server_read(request);
}

void server_close(server *server)
{
  if (server->fd == -1)
    return;

  assert(server->accept);
  reactor_cancel(server->accept, NULL, NULL);
  server->accept = 0;
  (void) reactor_close(NULL, NULL, server->fd);
  server->fd = -1;

  while (!list_empty(&server->requests))
    server_disconnect(list_front(&server->requests));

  timer_clear(&server->timer);
}

void server_respond(server_request *request, data status, data type, data body)
{
  assert(request->in_progress);
  request->in_progress = false;
  http_write_response(&request->stream.output.data_next, status, request->server->date, type, body);
  if (!request->abort)
    server_flush(request);
}

void server_disconnect(server_request *request)
{
  if (request->abort)
    *request->abort = true;
  (void) reactor_close(NULL, NULL, request->stream.fd);
  stream_destruct(&request->stream);
  list_erase(request, NULL);
}
