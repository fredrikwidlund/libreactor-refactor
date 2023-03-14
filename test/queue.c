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

  queue_consumer_construct(&c, NULL, NULL);
  queue_consumer_open(&c, &q, 1);
  assert_int_equal(*(int *) queue_consumer_pop_sync(&c), 1);
  assert_int_equal(*(int *) queue_consumer_pop_sync(&c), 2);
  assert_int_equal(*(int *) queue_consumer_pop_sync(&c), 3);
  queue_consumer_destruct(&c);
  queue_destruct(&q);
}

struct state
{
  queue queue;
  queue_producer producer;
  queue_consumer consumer;
  uint64_t value;
  size_t   calls;
};

static void callback(reactor_event *event)
{
  struct state *s = event->state;

  s->calls++;
  if (s->calls == s->value)
  {
    queue_producer_push(&s->producer, (int[]){0});
    queue_producer_close(&s->producer);
    queue_consumer_close(&s->consumer);
  }
}

extern void queue_producer_write(reactor_event *);
extern void queue_consumer_read(reactor_event *);

static void test_queue(__attribute__((unused)) void **arg)
{
  struct state s = {.value = 1000};
  int i;

  reactor_construct();
  queue_construct(&s.queue, sizeof (int));
  queue_consumer_construct(&s.consumer, callback, &s);
  queue_consumer_open(&s.consumer, &s.queue, 1);
  queue_consumer_pop(&s.consumer);
  queue_consumer_pop(&s.consumer);
  queue_producer_construct(&s.producer);
  queue_producer_open(&s.producer, &s.queue);
  for (i = 0; i < (int) s.value; i ++)
    queue_producer_push(&s.producer, &i);
  reactor_loop();
  queue_producer_destruct(&s.producer);
  queue_consumer_destruct(&s.consumer);
  queue_destruct(&s.queue);
  reactor_destruct();
  assert_int_equal(s.calls, 1000);

  /* cancel read */
  reactor_construct();
  queue_construct(&s.queue, sizeof (int));
  queue_consumer_construct(&s.consumer, callback, &s);
  queue_consumer_open(&s.consumer, &s.queue, 1);
  queue_consumer_pop(&s.consumer);
  queue_consumer_destruct(&s.consumer);
  queue_destruct(&s.queue);
  reactor_destruct();

  /* producer errors */
  reactor_construct();
  queue_construct(&s.queue, sizeof (int));
  queue_producer_construct(&s.producer);
  queue_producer_open(&s.producer, &s.queue);
  queue_producer_write((reactor_event[]){reactor_event_define(&s.producer, REACTOR_CALL, -EINTR)});
  expect_assert_failure(queue_producer_write((reactor_event[]){reactor_event_define(&s.producer, REACTOR_CALL, -EBADF)}));
  expect_assert_failure(queue_producer_write((reactor_event[]){reactor_event_define(&s.producer, REACTOR_CALL, 1)}));
  queue_producer_destruct(&s.producer);
  queue_destruct(&s.queue);
  reactor_destruct();

  /* consumer errors */
  reactor_construct();
  queue_construct(&s.queue, sizeof (int));
  queue_consumer_construct(&s.consumer, NULL, NULL);
  queue_consumer_open(&s.consumer, &s.queue, 1);
  queue_consumer_read((reactor_event[]){reactor_event_define(&s.consumer, REACTOR_CALL, -EINTR)});
  expect_assert_failure(queue_consumer_read((reactor_event[]){reactor_event_define(&s.consumer, REACTOR_CALL, -EBADF)}));
  expect_assert_failure(queue_consumer_read((reactor_event[]){reactor_event_define(&s.consumer, REACTOR_CALL, 1)}));
  queue_consumer_destruct(&s.consumer);
  queue_destruct(&s.queue);
  reactor_destruct();

}

int main()
{
  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test(test_queue_sync),
      cmocka_unit_test(test_queue)
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
