#include <stdio.h>
#include <threads.h>
#include <reactor.h>

static void async(reactor_event *event)
{
  switch (event->type)
  {
  case REACTOR_CALL:
    (void) printf("called thread 0x%08lx\n", thrd_current());
    break;
  case REACTOR_RETURN:
    (void) printf("return to thread 0x%08lx\n", thrd_current());
    break;
  }
}

int main()
{
  (void) printf("main thread 0x%08lx\n", thrd_current());
  reactor_construct();
  reactor_async(async, NULL);
  reactor_cancel(reactor_async(async, NULL), NULL, NULL);
  reactor_loop();
  reactor_destruct();
}
