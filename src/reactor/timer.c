#include <linux/time_types.h>
#include <linux/io_uring.h>

#include "../reactor.h"

static void timer_timeout(reactor_event *);

static void timer_submit(timer *timer)
{
  timer->tv.tv_sec = timer->time / 1000000000ULL;
  timer->tv.tv_nsec = timer->time %  1000000000ULL;
  timer->timeout = reactor_timeout(timer_timeout, timer, &timer->tv, 0, IORING_TIMEOUT_ABS | IORING_TIMEOUT_REALTIME);
}

static void timer_timeout(reactor_event *event)
{
  timer *timer = event->state;
  reactor_time time;

  time = timer->time;
  timer->timeout = 0;
  if (timer->delay)
  {
    timer->time += timer->delay;
    timer_submit(timer);
  }
  reactor_call(&timer->user, TIMER_TIMEOUT, (uintptr_t) &time);
}

void timer_construct(timer *timer, reactor_callback *callback, void *state)
{
  *timer = (struct timer) {.user = reactor_user_define(callback, state)};
}

void timer_destruct(timer *timer)
{
  timer_clear(timer);
}

void timer_set(timer *timer, reactor_time time, reactor_time delay)
{
  timer_clear(timer);
  timer->time = time;
  timer->delay = delay;
  timer_submit(timer);
}

void timer_clear(timer *timer)
{
  if (timer->timeout)
  {
    reactor_cancel(timer->timeout, NULL, NULL);
    timer->timeout = 0;
  }
}
