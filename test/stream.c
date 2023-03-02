#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <errno.h>
#include <limits.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cmocka.h>

#include "reactor.h"

struct state
{
  uint64_t value;
  size_t   calls;
  stream   in;
  stream   out;
};

static void callback(reactor_event *event)
{
  struct state *state = event->state;
  data data;

  state->calls++;

  switch (event->type)
  {
  case STREAM_READ:
    data = stream_data(&state->out);
    stream_consume(&state->out, data_size(data));
    stream_read(&state->out, 4096);
    break;
  case STREAM_FLUSHED:
    close(stream_fd(&state->in));
    stream_close(&state->in);
    break;
  default:
    break;
  }
}

static void test_domain_socket(__attribute__((unused)) void **arg)
{
  struct state state = {0};
  int fd[2];

  assert_true(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);

  reactor_construct();
  stream_construct(&state.in, callback, &state);
  stream_open(&state.in, fd[0], 0);
  stream_construct(&state.out, callback, &state);
  stream_open(&state.out, fd[1], 0);
  stream_write(&state.in, data_string("test"));
  stream_read(&state.out, 4096);
  stream_flush(&state.in);
  stream_flush(&state.in);
  stream_write(&state.in, data_string("test"));
  stream_flush(&state.in);
  reactor_loop();
  stream_destruct(&state.in);
  stream_destruct(&state.out);
  reactor_destruct();
  assert_int_equal(state.calls, 4);
  close(fd[0]);
  close(fd[1]);
}

static void test_pipe(__attribute__((unused)) void **arg)
{
  struct state state = {0};
  int fd[2];

  assert_true(pipe(fd) == 0);

  reactor_construct();
  stream_construct(&state.in, callback, &state);
  stream_open(&state.in, fd[0], 0);
  stream_construct(&state.out, callback, &state);
  stream_open(&state.out, fd[1], 0);
  stream_write(&state.in, data_string("test"));
  stream_read(&state.out, 4096);
  stream_flush(&state.in);
  stream_flush(&state.in);
  reactor_loop();
  stream_destruct(&state.in);
  stream_destruct(&state.out);
  reactor_destruct();
  assert_int_equal(state.calls, 2);
  close(fd[0]);
  close(fd[1]);
}

static void test_close(__attribute__((unused)) void **arg)
{
  struct state state = {0};
  int fd[2];

  assert_true(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);

  reactor_construct();
  stream_construct(&state.in, callback, &state);
  stream_open(&state.in, fd[0], 0);
  stream_read(&state.in, 4096);
  stream_write(&state.in, data_string("test"));
  stream_flush(&state.in);
  stream_close(&state.in);
  reactor_loop();
  stream_destruct(&state.in);
  reactor_destruct();
  assert_int_equal(state.calls, 0);
  close(fd[0]);
  close(fd[1]);
}

static void test_recv(__attribute__((unused)) void **arg)
{
  struct state state = {0};
  int fd[2];

  assert_true(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);

  reactor_construct();
  stream_construct(&state.in, callback, &state);
  stream_open(&state.in, fd[0], STREAM_TYPE_SOCKET);
  stream_read(&state.in, 4096);
  stream_close(&state.in);
  reactor_loop();

  /* read from bad file descriptor */
  stream_read(&state.in, 4096);
  reactor_loop();

  /* send to bad file descriptor */
  stream_write(&state.in, data_string("test"));
  stream_flush(&state.in);
  reactor_loop();

  stream_destruct(&state.in);
  stream_destruct(&state.out);
  reactor_destruct();
  assert_int_equal(state.calls, 2);
  close(fd[0]);
  close(fd[1]);
}

static void test_transfer(__attribute__((unused)) void **arg)
{
  struct state state = {0};
  int fd[2];
  size_t size;
  char *buffer;

  assert_true(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);

  size = 128 * 1024 * 1024;
  buffer = calloc(1, size);

  reactor_construct();
  stream_construct(&state.in, callback, &state);
  stream_open(&state.in, fd[0], 0);
  stream_construct(&state.out, callback, &state);
  stream_open(&state.out, fd[1], 0);
  stream_write(&state.in, data_define(buffer, size));
  stream_read(&state.out, 4096);
  stream_flush(&state.in);
  stream_flush(&state.in);
  stream_write(&state.in, data_string("test"));
  stream_flush(&state.in);
  reactor_loop();
  stream_destruct(&state.in);
  stream_destruct(&state.out);
  reactor_destruct();
  close(fd[0]);
  close(fd[1]);
  free(buffer);
}

int main()
{
  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test(test_domain_socket),
      cmocka_unit_test(test_pipe),
      cmocka_unit_test(test_close),
      cmocka_unit_test(test_recv),
      cmocka_unit_test(test_transfer),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
