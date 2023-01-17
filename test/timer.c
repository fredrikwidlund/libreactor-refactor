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

  state->calls++;
}

static void test_timer(__attribute__((unused)) void **arg)
{
  struct state state = {0};
  timer t;

  reactor_construct();
  timer_construct(&t, callback, &state);

  timer_set(&t, reactor_now(), 0);
  reactor_loop_once();

  timer_set(&t, reactor_now(), 1);
  reactor_loop_once();

  timer_set(&t, reactor_now(), 0);
  timer_destruct(&t);
  reactor_loop_once();

  reactor_destruct();
  assert_int_equal(state.calls, 2);
}

int main()
{
  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test(test_timer),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
