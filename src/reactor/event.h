#ifndef REACTOR_EVENT_H_INCLUDED
#define REACTOR_EVENT_H_INCLUDED

#include <stdbool.h>

#include "../reactor.h"

enum
{
  EVENT_SIGNAL
};

typedef struct event event;

struct event
{
  reactor_user  user;
  reactor_id    read;
  uint64_t      counter;
  int           fd;
  bool         *abort;
};

void event_construct(event *, reactor_callback *, void *);
void event_open(event *);
void event_signal(event *);
void event_signal_sync(event *);
void event_close(event *);
void event_destruct(event *);

#endif /* REACTOR_EVENT_H_INCLUDED */

