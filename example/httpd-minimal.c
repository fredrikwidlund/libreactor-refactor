#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>
#include <err.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <reactor.h>

const char reply[] =
  "HTTP/1.1 200 OK\r\n"
  "Server: libreactor\r\n"
  "Date: Fri, 27 May 2022 20:36:30 GMT\r\n"
  "Content-Type: text/plain\r\n"
  "Content-Length: 13\r\n"
  "\r\n"
  "Hello, World!";

struct connection
{
  int socket;
  char input[1024];
};

struct state
{
  int socket;
};

static int server_socket(int port)
{
  int s;

  s = socket(AF_INET, SOCK_STREAM, 0);
  assert(s >= 0);
  assert(setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (int[]) {1}, sizeof(int)) == 0);
  assert(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (int[]) {1}, sizeof(int)) == 0);
  assert(bind(s, (struct sockaddr *) (struct sockaddr_in[]) {{.sin_family = AF_INET, .sin_port = htons(port)}},
              sizeof(struct sockaddr_in)) == 0);
  assert(listen(s, INT_MAX) == 0);
  return s;
}

static void recv_callback(reactor_event *event)
{
  struct connection *connection = event->state;
  int result = event->value;

  if (result <= 0)
  {
    (void) close(connection->socket);
    free(connection);
    return;
  }

  reactor_send(NULL, NULL, connection->socket, reply, strlen(reply), 0);
  reactor_recv(recv_callback, connection, connection->socket, connection->input, sizeof connection->input, 0);
}

static void accept_callback(reactor_event *event)
{
  struct state *state = event->state;
  struct connection *connection = malloc(sizeof *connection);

  connection->socket = event->value;
  printf("accept %d\n", connection->socket);
  reactor_recv(recv_callback, connection, connection->socket, connection->input, sizeof connection->input, 0);
  reactor_accept(accept_callback, state, state->socket, NULL, NULL, 0);
}

int main()
{
  struct state state = {0};

  state.socket = server_socket(80);

  reactor_accept(accept_callback, &state, state.socket, NULL, NULL, 0);
  reactor_loop();
}
