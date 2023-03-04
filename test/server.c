#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <errno.h>
#include <limits.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cmocka.h>

#include "reactor.h"

static int server_socket(int *port)
{
  struct sockaddr_in sin = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(0x7f000001)};
  socklen_t len = sizeof sin;
  int s;

  s = socket(AF_INET, SOCK_STREAM, 0);
  assert(s >= 0);
  assert(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (int[]) {1}, sizeof(int)) == 0);
  assert(bind(s, (struct sockaddr *) &sin, sizeof sin) == 0);
  assert(listen(s, INT_MAX) == 0);
  assert(getsockname(s, (struct sockaddr *) &sin, &len) == 0);
  *port = ntohs(sin.sin_port);
  return s;
}

static int client_socket(int port)
{
  struct sockaddr_in sin = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(0x7f000001), .sin_port = htons(port)};
  socklen_t len = sizeof sin;
  int c;

  c = socket(AF_INET, SOCK_STREAM, 0);
  assert(c >= 0);
  assert(connect(c, (struct sockaddr *) &sin, len) == 0);
  return c;
}

struct state
{
  server   server;
  stream   stream;
  int      result;
  uint64_t value;
  size_t   calls;
};

static void server_next(reactor_event *event)
{
  server_request *request = (server_request *) event->state;

  server_respond(request, data_string("200 OK"), data_string("text/plain"), data_string("ok"));
}

static void server_callback(reactor_event *event)
{
  struct state *state = event->state;
  server_request *request = (server_request *) event->data;

  state->calls++;
  assert(event->type == SERVER_REQUEST);
  if (data_equal(request->target, data_string("/abort")))
    server_disconnect(request);
  else if (data_equal(request->target, data_string("/next")))
    reactor_next(server_next, request);
  else if (data_equal(request->target, data_string("/null")))
    ;
  else
    server_respond(request, data_string("200 OK"), data_string("text/plain"), data_string("ok"));

  state->calls++;
}

static void stream_callback(reactor_event *event)
{
  struct state *state = event->state;
  data data = stream_data(&state->stream);
  int code;

  state->calls++;
  switch (event->type)
  {
  case STREAM_READ:
    assert_true(memchr(data_base(data), '\n', data_size(data)) != NULL);
    assert_true(strncmp(data_base(data), "HTTP/1.1 ", 9) == 0);
    data = data_offset(data, 9);
    code = strtoul(data_base(data), NULL, 0);
    assert_int_equal(code, state->result);
    server_close(&state->server);
    break;
  case STREAM_FLUSHED:
    stream_read(&state->stream, 65536);
    break;
  case STREAM_CLOSE:
    assert_int_equal(state->result, 0);
    server_close(&state->server);
    break;
  }

  state->calls++;
}

static void run(char *request, int result, int calls)
{
  struct state state = {.result = result};
  int c, s, port;

  reactor_construct();
  server_construct(&state.server, server_callback, &state);

  s = server_socket(&port);
  server_open(&state.server, s);

  stream_construct(&state.stream, stream_callback, &state);
  c = client_socket(port);
  stream_open(&state.stream, c, STREAM_TYPE_SOCKET);

  if (request)
  {
    stream_write(&state.stream, data_string(request));
    stream_flush(&state.stream);
  }
  else
  {
    reactor_loop_once();
    shutdown(stream_fd(&state.stream), SHUT_RDWR);
    stream_close(&state.stream);
    reactor_loop_once();
    server_close(&state.server);
  }
  reactor_loop();
  stream_destruct(&state.stream);
  server_destruct(&state.server);
  reactor_destruct();
  close(c);
  close(s);
  assert_int_equal(state.calls, calls);
}

static void test_server(__attribute__((unused)) void **arg)
{
  run("GET / HTTP/1.0\r\n\r\n", 200, 6);
  run("GET / HTTP/1.0\r\n\r\nGET", 200, 6);
  run("GET /abort HTTP/1.0\r\n\r\n", 0, 6);
  run("GET /next HTTP/1.0\r\n\r\n", 200, 6);
  run("GET / HTTP/1.0\r\n\r\nGET /null HTTP/1.0\r\n\r\n", 200, 8);
  run("i n v a l i d", 0, 4);
  run(NULL, 0, 0);
}

static void test_error(__attribute__((unused)) void **arg)
{
  server server;

  reactor_construct();
  server_construct(&server, NULL, NULL);
  server_accept(&server, -1);
  server_destruct(&server);
  reactor_destruct();
}

int main()
{
  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test(test_server),
      cmocka_unit_test(test_error),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
