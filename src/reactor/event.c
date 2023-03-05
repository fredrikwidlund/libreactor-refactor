#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <sys/eventfd.h>

#include "../reactor.h"

static void event_read(reactor_event *reactor_event)
{
  event *event = reactor_event->state;
  bool abort = false;

  event->read = 0;
  assert(reactor_event->data == sizeof event->counter);
  event->abort = &abort;
  reactor_call(&event->user, EVENT_SIGNAL, event->counter);
  if (abort)
    return;
  event->abort = NULL;
  event->read = reactor_read(event_read, event, event->fd, &event->counter, sizeof event->counter, 0);
}

void event_construct(event *event, reactor_callback *callback, void *state)
{
  *event = (struct event) {.user = reactor_user_define(callback, state), .fd = -1};
}

void event_open(event *event)
{
  event->fd = eventfd(0, event->counter);
  event->read = reactor_read(event_read, event, event->fd, &event->counter, sizeof event->counter, 0);
}

void event_signal(event *event)
{
  static const uint64_t signal = 1;

  (void) reactor_write(NULL, NULL, event->fd, &signal, sizeof signal, 0);
}

void event_signal_sync(event *event)
{
  ssize_t n;

  n = write(event->fd, (uint64_t[]) {1}, sizeof (uint64_t));
  assert(n == sizeof (uint64_t));
}

void event_close(event *event)
{
  if (event->fd == -1)
    return;

  if (event->abort)
    *event->abort = true;
  if (event->read)
    reactor_cancel(event->read, NULL, NULL);
  (void) reactor_close(NULL, NULL, event->fd);
  event->fd = -1;
}

void event_destruct(event *event)
{
  event_close(event);
}
