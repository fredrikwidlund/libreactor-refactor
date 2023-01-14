#include <stdio.h>
#include <reactor.h>

static void next(reactor_event *event)
{
  (void) event;
  printf("next\n");
}

int main()
{
  reactor_construct();
  reactor_next(next, NULL);
  reactor_nop(next, NULL);
  reactor_cancel(reactor_next(next, NULL), NULL, NULL);
  reactor_loop();
  reactor_destruct();
}
