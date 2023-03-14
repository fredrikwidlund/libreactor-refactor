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

static void test_queue_sync(__attribute__((unused)) void **arg)
{
  queue q;
  queue_producer p;
  queue_consumer c;
  int *x;

  queue_construct(&q, sizeof *x);
  queue_producer_construct(&p);
  queue_producer_open(&p, &q);
  queue_producer_push_sync(&p, (int[]) {1});
  queue_producer_push_sync(&p, (int[]) {2});
  queue_producer_push_sync(&p, (int[]) {3});
  queue_producer_destruct(&p);

  queue_consumer_construct(&c, callback, &c);
  queue_consumer_open(&c, &q, 1);
  assert_int_equal(*(int *) queue_consumer_pop_sync(&c), 1);
  assert_int_equal(*(int *) queue_consumer_pop_sync(&c), 2);
  assert_int_equal(*(int *) queue_consumer_pop_sync(&c), 3);
  queue_consumer_destruct(&c);
  queue_destruct(&q);
}

int main()
{
  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test(test_queue_sync),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
