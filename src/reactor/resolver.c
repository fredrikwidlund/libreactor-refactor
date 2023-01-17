#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../reactor.h"

static void resolver_async(reactor_event *event)
{
  resolver *resolver = event->state;
  struct addrinfo *ai;

  if (event->type == REACTOR_CALL)
    resolver->code = getaddrinfo(resolver->host, resolver->port, &resolver->hints, &resolver->result);
  else
  {
    resolver->call = 0;
    ai = resolver->result;
    free(resolver->host);
    free(resolver->port);
    resolver->result = NULL;
    reactor_call(&resolver->user, resolver->code == 0 ? RESOLVER_OK : RESOLVER_ERROR, (uint64_t) ai);
    if (ai)
      freeaddrinfo(ai);
  }
}

void resolver_construct(resolver *resolver, reactor_callback *callback, void *state)
{
  *resolver = (struct resolver) {.user = reactor_user_define(callback, state)};
}

void resolver_destruct(resolver *resolver)
{
  (void) resolver;
}

void resolver_lookup(resolver *resolver, int flags, int family, int socktype, int protocol, char *host, char *port)
{
  assert(!resolver->call);
  resolver->hints = (struct addrinfo) {.ai_flags = flags, .ai_family = family, .ai_socktype = socktype, .ai_protocol = protocol};
  resolver->host = strdup(host);
  resolver->port = strdup(port);
  resolver->call = reactor_async(resolver_async, resolver);
}
