#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <reactor.h>

struct task
{
  server_request *request;
  timer           timer;
};

static int create_server_socket(int port)
{
  struct sockaddr_in sin = {.sin_family = AF_INET, .sin_addr.s_addr = 0, .sin_port = htons(port)};
  int fd, e;

  fd = socket(AF_INET, SOCK_STREAM, PF_UNSPEC);
  (void) setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (int[]) {1}, sizeof(int));
  (void) setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (int[]) {1}, sizeof(int));
  e = bind(fd, (struct sockaddr *) &sin, sizeof sin);
  assert(e == 0);
  e = listen(fd, INT_MAX);
  assert(e == 0);
  return fd;
}

static void timer_callback(reactor_event *event)
{
  struct task *task = event->state;

  server_respond(task->request, data_string("200 OK"), data_string("text/plain"), data_string("Timeout!"));
  timer_destruct(&task->timer);
  free(task);
}

static void server_callback(reactor_event *event)
{
  server_request *request = (server_request *) event->data;
  struct task *task;

  if (data_equal(request->target, data_string("/wait")))
  {
    task = malloc(sizeof *task);
    task->request = request;
    timer_construct(&task->timer, timer_callback, task);
    timer_set(&task->timer, reactor_now() + 1000000000, 0);
  }
  else if (data_equal(request->target, data_string("/disconnect")))
    server_disconnect(request);
  else if (data_equal(request->target, data_string("/quit")))
    server_close(request->server);
  else if (data_equal(request->target, data_string("/")))
    server_respond(request, data_string("200 OK"), data_string("text/plain"), data_string("Good morning!"));
  else
    server_respond(request, data_string("404 Not Found"), data_null(), data_null());
}

int main()
{
  server server;

  reactor_construct();
  server_construct(&server, server_callback, &server);
  server_open(&server, create_server_socket(80));
  reactor_loop();
  reactor_destruct();
}
