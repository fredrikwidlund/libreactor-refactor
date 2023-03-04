#ifndef REACTOR_SERVER_H_INCLUDED
#define REACTOR_SERVER_H_INCLUDED

#include "../reactor.h"

enum
{
  SERVER_REQUEST
};

typedef struct server         server;
typedef struct server_request server_request;

struct server
{
  reactor_user   user;
  int            fd;
  reactor_id     accept;
  list           requests;
  char           date_string[30];
  data           date;
};

struct server_request
{
  struct server *server;
  size_t         stream_offset;
  stream         stream;
  bool          *abort;
  bool           in_progress;

  data           method;
  data           target;
  data           body;
  http_field     fields[16];
  size_t         fields_count;
};

void server_construct(server *, reactor_callback *, void *);
void server_destruct(server *);
void server_open(server *, int);
void server_accept(server *, int);
void server_close(server *);
void server_respond(server_request *, data, data, data);
void server_disconnect(server_request *);

#endif /* REACTOR_SERVER_H_INCLUDED */
