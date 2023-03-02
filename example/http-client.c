#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <err.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <reactor.h>

const char request[] =
  "GET / HTTP/1.1\r\n"
  "Host: localhost\r\n"
  "\r\n";

struct state
{
  int seconds;
  size_t requests;
};

struct client
{
  int socket;
  char input[1024];
  struct state *state;
};

static struct addrinfo *resolve_address(char *host, char *port)
{
  int e;
  struct addrinfo *ai;

  e = getaddrinfo(host, port, NULL, &ai);
  if (e == -1 && ai)
    {
      freeaddrinfo(ai);
      ai = NULL;
    }
  return ai;
}

static void client_recv(reactor_event *event)
{
  struct client *client = event->state;
  int result = event->data;

  assert(result > 0);
  client->state->requests++;
  reactor_send(NULL, NULL, client->socket, request, strlen(request), 0);
  reactor_recv(client_recv, client, client->socket, client->input, sizeof client->input, 0);
}

static void client_connect(reactor_event *event)
{
  struct client *client = event->state;
  int result = event->data;

  assert(result == 0);
  reactor_send(NULL, NULL, client->socket, request, strlen(request), 0);
  reactor_recv(client_recv, client, client->socket, client->input, sizeof client->input, 0);
}

static struct client *client_create(struct state *state, struct addrinfo *ai)
{
  struct client *client;

  client = malloc(sizeof *client);
  client->state = state;
  client->socket = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  if (client->socket == -1)
    err(1, "socket");

  reactor_connect(client_connect, client, client->socket, ai->ai_addr, ai->ai_addrlen);
  return client;
}

static void state_timeout(reactor_event *event)
{
  struct state *state = event->state;

  printf("requests per seconds: %lu\n", state->requests / state->seconds);
  exit(0);
}

int main()
{
  struct state state = {.seconds = 5};
  struct addrinfo *ai = resolve_address("127.0.0.1", "80");
  struct timespec tv = {.tv_sec = state.seconds};
  int i, n = 256;

  reactor_construct();
  printf("running %d clients for %d seconds\n", n, state.seconds);
  for (i = 0; i < n; i ++)
    client_create(&state, ai);

  reactor_timeout(state_timeout, &state, &tv, 0, 0);
  reactor_loop();
  freeaddrinfo(ai);
  reactor_destruct();
}
