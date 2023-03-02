#include <stdio.h>
#include <assert.h>
#include <reactor.h>

static void receive(reactor_event *event)
{
  printf("receive %d\n", (int) event->data);
}

static void cancel(reactor_event *event)
{
  printf("cancel %d\n", (int) event->data);
}

int main()
{
  char buf[256];
  int fd[2];
  reactor_id id;

  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);

  reactor_construct();
  id = reactor_recv(receive, NULL, fd[0], buf, sizeof buf, 0);
  reactor_async_cancel(cancel, NULL, id);
  //shutdown(fd[0], 0);
  reactor_loop();
  reactor_destruct();
}
