#ifndef REACTOR_NOTIFY_H_INCLUDED
#define REACTOR_NOTIFY_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include <sys/inotify.h>

#include "../reactor.h"

enum notify_event_type
{
  NOTIFY_EVENT
};

typedef struct notify_event notify_event;
typedef struct notify_entry notify_entry;
typedef struct notify       notify;

struct notify_event
{
  int           id;
  char         *path;
  char         *name;
  uint32_t      mask;
};

struct notify_entry
{
  int           id;
  char         *path;
};

struct notify
{
  reactor_user  user;
  int           fd;
  reactor_id    read;
  reactor_id    next;
  list          entries;
  mapi          ids;
  maps          paths;
  bool         *abort;
  char          buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
};

void notify_construct(notify *, reactor_callback *, void *);
void notify_open(notify *);
int  notify_add(notify *, char *, uint32_t);
bool notify_remove_path(notify *, char *);
bool notify_remove_id(notify *, int);
void notify_close(notify *);
void notify_destruct(notify *);

#endif /* REACTOR_NOTIFY_H_INCLUDED */
