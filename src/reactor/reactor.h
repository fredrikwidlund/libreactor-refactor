#ifndef REACTOR_REACTOR_H
#define REACTOR_REACTOR_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <sys/socket.h>

#define REACTOR_RING_SIZE 1024

typedef struct reactor_event  reactor_event;
typedef struct reactor_user   reactor_user;
typedef uint64_t              reactor_id;
typedef void                 (reactor_callback)(reactor_event *);

struct reactor_event
{
  void            *state;
  int              type;
  uint64_t         value;
};

struct reactor_user
{
  reactor_callback *callback;
  void             *state;
};

void       reactor_construct(void);
void       reactor_destruct(void);
void       reactor_loop(void);
void       reactor_loop_once(void);

reactor_id reactor_recv(reactor_callback *, void *, int, void *, size_t, int);
reactor_id reactor_read(reactor_callback *, void *, int, void *, size_t, size_t);
reactor_id reactor_send(reactor_callback *, void *, int, const void *, size_t, int);
reactor_id reactor_timeout(reactor_callback *, void *, struct timespec *, int, int);
reactor_id reactor_accept(reactor_callback *, void *, int, struct sockaddr *, socklen_t *, int);

#endif /* REACTOR_REACTOR_H */
