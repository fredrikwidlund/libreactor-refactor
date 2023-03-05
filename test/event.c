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
  event    event;
  uint64_t value;
  size_t   calls;
  bool     close;
};

static void callback(reactor_event *event)
{
  struct state *state = event->state;

  state->calls++;
  if (state->close)
    event_close(&state->event);
}

static void test_event(__attribute__((unused)) void **arg)
{
  struct state state = {0};

  reactor_construct();
  event_construct(&state.event, callback, &state);
  event_open(&state.event);
  event_signal(&state.event);
  reactor_loop_once();
  assert_int_equal(state.calls, 1);
  event_close(&state.event);
  event_open(&state.event);
  event_signal_sync(&state.event);
  state.close = true;
  reactor_loop_once();
  assert_int_equal(state.calls, 2);
  event_destruct(&state.event);
  reactor_destruct();
}

int main()
{
  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test(test_event),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
