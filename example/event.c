#include <reactor.h>

static void callback(reactor_event *reactor_event)
{
  event *event = reactor_event->state;

  event_close(event);
}

int main()
{
  event e;

  reactor_construct();
  event_construct(&e, callback, &e);
  event_open(&e);
  event_signal(&e);
  reactor_loop();
  reactor_destruct();
}
