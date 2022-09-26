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
};

static void callback(reactor_event *event)
{
  struct state *state = event->state;

  assert_int_equal((int) event->value, state->value);
  state->calls++;
}

static void test_construct(__attribute__((unused)) void **arg)
{
  reactor_construct();
  reactor_destruct();
}

static void test_loop(__attribute__((unused)) void **arg)
{
  reactor_loop();
  reactor_loop_once();
}

static void test_recv(__attribute__((unused)) void **arg)
{
  struct state state = {.value = 6};
  static char buffer[1024] = {0};
  int fd[2];

  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
  assert(write(fd[1], "test2", 6) == 6);
  reactor_recv(callback, &state, fd[0], buffer, sizeof buffer, 0);
  reactor_loop();
  assert_int_equal(state.calls, 1);
  close(fd[0]);
  close(fd[1]);
}

static void test_read(__attribute__((unused)) void **arg)
{
  struct state state = {.value = 6};
  static char buffer[1024] = {0};
  int fd[2];

  assert(pipe(fd) == 0);
  assert(write(fd[1], "test2", 6) == 6);
  reactor_read(callback, &state, fd[0], buffer, sizeof buffer, 0);
  reactor_loop();
  assert_int_equal(state.calls, 1);
  close(fd[0]);
  close(fd[1]);
}

static void test_send(__attribute__((unused)) void **arg)
{
  struct state state = {.value = 4};
  int fd[2];

  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
  reactor_send(callback, &state, fd[1], "test", 4, 0);
  reactor_loop();
  assert_int_equal(state.calls, 1);
  close(fd[0]);
  close(fd[1]);
}

static void test_timeout(__attribute__((unused)) void **arg)
{
  struct state state = {.value = -ETIME};
  static struct timespec tv = {0};
  int i;

  reactor_timeout(NULL, NULL, &tv, 0, 0);
  reactor_loop();
  assert_int_equal(state.calls, 0);

  reactor_timeout(callback, &state, &tv, 0, 0);
  reactor_loop();
  assert_int_equal(state.calls, 1);

  for (i = 0; i < REACTOR_RING_SIZE + 1; i++)
    reactor_timeout(callback, &state, &tv, 0, 0);
  reactor_loop();
  assert_int_equal(state.calls, REACTOR_RING_SIZE + 1 + 1);
}

static void test_accept_callback(reactor_event *event)
{
  int *calls = event->state;
  int fd = event->value;

  assert_true(fd >= 0);
  (*calls)++;
}

static void test_accept(__attribute__((unused)) void **arg)
{
  int s, c, calls = 0;
  struct sockaddr_in sin;
  socklen_t len = sizeof sin;

  s = socket(AF_INET, SOCK_STREAM, 0);
  assert(s >= 0);
  assert(setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (int[]) {1}, sizeof(int)) == 0);
  assert(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (int[]) {1}, sizeof(int)) == 0);
  assert(bind(s, (struct sockaddr *) (struct sockaddr_in[]) {{.sin_family = AF_INET}}, sizeof(struct sockaddr_in)) == 0);
  assert(listen(s, INT_MAX) == 0);
  assert(getsockname(s, (struct sockaddr *) &sin, &len) == 0);

  c = socket(AF_INET, SOCK_STREAM, 0);
  assert(c >= 0);
  assert(connect(c, (struct sockaddr *) &sin, len) == 0);

  reactor_accept(test_accept_callback, &calls, s, NULL, NULL, 0);
  reactor_loop();
  assert_int_equal(calls, 1);
  close(s);
  close(c);
}

int main()
{
  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test(test_construct),
      cmocka_unit_test(test_loop),
      cmocka_unit_test(test_recv),
      cmocka_unit_test(test_read),
      cmocka_unit_test(test_send),
      cmocka_unit_test(test_timeout),
      cmocka_unit_test(test_accept)
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
