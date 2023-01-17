#ifndef REACTOR_TIMER_H_INCLUDED
#define REACTOR_TIMER_H_INCLUDED

#include <linux/time_types.h>

#include "../reactor.h"

enum
{
  TIMER_TIMEOUT
};

typedef struct timer timer;

struct timer
{
  reactor_user             user;
  reactor_time             time;
  reactor_time             delay;
  reactor_id               timeout;
  struct timespec          tv;
};

void timer_construct(timer *, reactor_callback *, void *);
void timer_destruct(timer *);
void timer_set(timer *, reactor_time, reactor_time);
void timer_clear(timer *);

#endif /* REACTOR_TIMER_H_INCLUDED */
