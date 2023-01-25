#ifndef REACTOR_REACTOR_H
#define REACTOR_REACTOR_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <sys/socket.h>

#define REACTOR_RING_SIZE 4096

enum
{
  REACTOR_CALL,
  REACTOR_RETURN
};

typedef struct reactor_event  reactor_event;
typedef struct reactor_user   reactor_user;
typedef uint64_t              reactor_time;
typedef uint64_t              reactor_id;
typedef void                 (reactor_callback)(reactor_event *);

struct reactor_event
{
  void            *state;
  int              type;
  uint64_t         data;
};

struct reactor_user
{
  reactor_callback *callback;
  void             *state;
};

reactor_event reactor_event_define(void *, int, uint64_t);
reactor_user  reactor_user_define(reactor_callback *, void *);
void          reactor_user_construct(reactor_user *, reactor_callback *, void *);

void          reactor_construct(void);
void          reactor_destruct(void);
reactor_time  reactor_now(void);
void          reactor_loop(void);
void          reactor_loop_once(void);
void          reactor_call(reactor_user *, int, uint64_t);
void          reactor_cancel(reactor_id, reactor_callback *, void *);

reactor_id    reactor_async(reactor_callback *, void *);
reactor_id    reactor_next(reactor_callback *, void *);
reactor_id    reactor_async_cancel(reactor_callback *, void *, uint64_t);

reactor_id    reactor_nop(reactor_callback *, void *);
reactor_id    reactor_readv(reactor_callback *, void *, int, const struct iovec *, int, size_t);
reactor_id    reactor_writev(reactor_callback *, void *, int, const struct iovec *, int, size_t);
reactor_id    reactor_fsync(reactor_callback *, void *, int);
reactor_id    reactor_poll_add(reactor_callback *, void *, int, short int);
reactor_id    reactor_poll_add_multi(reactor_callback *, void *, int, short int);
reactor_id    reactor_poll_update(reactor_callback *, void *, reactor_id, short int);
reactor_id    reactor_poll_remove(reactor_callback *, void *, reactor_id);

/* reactor_poll_multi */
/* reactor_poll_remove */
/* reactor_epoll_ctl */
reactor_id    reactor_send(reactor_callback *, void *, int, const void *, size_t, int);
reactor_id    reactor_recv(reactor_callback *, void *, int, void *, size_t, int);
reactor_id    reactor_read(reactor_callback *, void *, int, void *, size_t, size_t);
reactor_id    reactor_write(reactor_callback *, void *, int, const void *, size_t, size_t);
reactor_id    reactor_timeout(reactor_callback *, void *, struct timespec *, int, int);
reactor_id    reactor_connect(reactor_callback *, void *, int, struct sockaddr *, socklen_t);
reactor_id    reactor_accept(reactor_callback *, void *, int, struct sockaddr *, socklen_t *, int);
reactor_id    reactor_close(reactor_callback *, void *, int);

#endif /* REACTOR_REACTOR_H */
