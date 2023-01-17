#ifndef REACTOR_RESOLVER_H
#define REACTOR_RESOLVER_H

#include "../reactor.h"
#include <netdb.h>

enum
{
  RESOLVER_OK,
  RESOLVER_ERROR
};

typedef struct resolver resolver;

struct resolver
{
  reactor_user     user;
  struct addrinfo  hints;
  char            *host;
  char            *port;
  reactor_id       call;
  int              code;
  struct addrinfo *result;
};

void resolver_construct(resolver *, reactor_callback *, void *);
void resolver_destruct(resolver *);
void resolver_lookup(resolver *, int, int, int, int, char *, char *);

#endif /* REACTOR_RESOLVER_H */
