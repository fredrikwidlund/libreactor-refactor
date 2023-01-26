#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <threads.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <linux/io_uring.h>

#include "../reactor.h"

/* reactor ring */

typedef struct reactor_ring reactor_ring;

struct reactor_ring
{
  int                  fd;
  size_t               size;
  size_t               ring_size;
  uint8_t             *ring;

  size_t               sqe_size;
  struct io_uring_sqe *sqe;
  uint32_t            *sq_mask;
  uint32_t            *sq_head;
  uint32_t            *sq_tail;
  uint32_t            *sq_array;

  struct io_uring_cqe *cqe;
  uint32_t            *cq_mask;
  uint32_t            *cq_head;
  uint32_t            *cq_tail;
};

static void reactor_ring_construct(reactor_ring *ring, size_t size)
{
  struct io_uring_params params = {0};
  int e;

  ring->size = size;
  ring->fd = syscall(SYS_io_uring_setup, ring->size, &params);
  assert(ring->fd >= 0);
  assert(params.features & IORING_FEAT_SINGLE_MMAP);

  ring->ring_size = params.sq_off.array + params.sq_entries * sizeof(unsigned);
  ring->ring = mmap(0, ring->ring_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring->fd, IORING_OFF_SQ_RING);
  assert(ring->ring != MAP_FAILED);
  e = madvise(ring->ring, ring->ring_size, MADV_DONTFORK);
  assert(e == 0);

  ring->sqe_size = params.sq_entries * sizeof(struct io_uring_sqe);
  ring->sqe = mmap(0, ring->sqe_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring->fd, IORING_OFF_SQES);
  assert(ring->sqe != MAP_FAILED);
  e = madvise(ring->sqe, ring->sqe_size, MADV_DONTFORK);
  assert(e == 0);

  ring->sq_mask = (uint32_t *) (ring->ring + params.sq_off.ring_mask);
  ring->sq_head = (uint32_t *) (ring->ring + params.sq_off.head);
  ring->sq_tail = (uint32_t *) (ring->ring + params.sq_off.tail);
  ring->sq_array = (uint32_t *) (ring->ring + params.sq_off.array);

  ring->cqe = (struct io_uring_cqe *) (ring->ring + params.cq_off.cqes);
  ring->cq_mask = (uint32_t *) (ring->ring + params.cq_off.ring_mask);
  ring->cq_head = (uint32_t *) (ring->ring + params.cq_off.head);
  ring->cq_tail = (uint32_t *) (ring->ring + params.cq_off.tail);
}

static void reactor_ring_destruct(reactor_ring *ring)
{
  int e;

  e = close(ring->fd);
  assert(e == 0);
  e = munmap(ring->ring, ring->ring_size);
  assert(e == 0);
  e = munmap(ring->sqe, ring->sqe_size);
  assert(e == 0);
}

static void reactor_ring_flush(reactor_ring *ring)
{
  uint32_t n, to_submit = *ring->sq_tail - *ring->sq_head;

  if (to_submit == ring->size)
  {
    n = syscall(__NR_io_uring_enter, ring->fd, to_submit, 0, IORING_ENTER_GETEVENTS, NULL, NULL);
    assert(n == to_submit);
  }
}

static struct io_uring_sqe *reactor_ring_sqe(reactor_ring *ring)
{
  unsigned i;

  reactor_ring_flush(ring);
  i = *ring->sq_tail & *ring->sq_mask;
  (*ring->sq_tail)++;
  ring->sq_array[i] = i;
  return &ring->sqe[i];
}

struct io_uring_cqe *reactor_ring_cqe(reactor_ring *ring)
{
  struct io_uring_cqe *cqe;

  if (*ring->cq_head == *ring->cq_tail)
    return NULL;
  cqe = &ring->cqe[*ring->cq_head & *ring->cq_mask];
  (*ring->cq_head)++;
  return cqe;
}

static void reactor_ring_update(reactor_ring *ring)
{
  int n, to_submit = *ring->sq_tail - *ring->sq_head;

  n = syscall(__NR_io_uring_enter, ring->fd, to_submit, 1, IORING_ENTER_GETEVENTS, NULL, NULL);
  assert(n == to_submit);
}

/* reactor_event */

reactor_event reactor_event_define(void *state, int type, uint64_t data)
{
  return (reactor_event) {.state = state, .type = type, .data = data};
}

/* reactor user */

static void reactor_user_callback(reactor_event *event)
{
  (void) event;
}

reactor_user reactor_user_define(reactor_callback *callback, void *state)
{
  return (reactor_user) {.callback = callback ? callback : reactor_user_callback, .state = state};
}

void reactor_user_construct(reactor_user *user, reactor_callback *callback, void *state)
{
  *user = reactor_user_define(callback, state);
}

/* reactor */

typedef struct reactor_async_state reactor_async_state;

struct reactor
{
  size_t               ref;
  reactor_ring         ring;
  pool                 users;
  vector               next;
  reactor_time         time;
};

struct reactor_async_state
{
  reactor_user         user;
  int                  signal;
  reactor_id           read;
  uint64_t             counter;
  thrd_t               id;
};

static __thread struct reactor reactor = {0};

static reactor_user *reactor_alloc_user(reactor_callback *callback, void *state)
{
  reactor_user *user = pool_malloc(&reactor.users);

  *user = reactor_user_define(callback, state);
  return user;
}

static void reactor_free_user(reactor_user *user)
{
  pool_free(&reactor.users, user);
}

static int reactor_async_call(void *arg)
{
  reactor_async_state *async = arg;
  ssize_t n;

  reactor_call(&async->user, REACTOR_CALL, 0);
  n = write(async->signal, (uint64_t[]){1}, sizeof async->counter);
  assert(n == sizeof async->counter);
  return 0;
}

static void reactor_async_return(reactor_event *event)
{
  reactor_async_state *async = event->state;

  reactor_call(&async->user, REACTOR_RETURN, 0);
  (void) reactor_close(NULL, NULL, async->signal);
  free(async);
}

void reactor_construct(void)
{
  if (!reactor.ref)
  {
    reactor_ring_construct(&reactor.ring, REACTOR_RING_SIZE);
    pool_construct(&reactor.users, sizeof(reactor_user));
    vector_construct(&reactor.next, sizeof (reactor_user *));
  }
  reactor.ref++;
}

void reactor_destruct(void)
{
  reactor.ref--;
  if (!reactor.ref)
  {
    reactor_ring_destruct(&reactor.ring);
    pool_destruct(&reactor.users);
    vector_destruct(&reactor.next, NULL);
  }
}

reactor_time reactor_now(void)
{
  struct timespec tv;

  if (!reactor.time)
  {
    clock_gettime(CLOCK_REALTIME_COARSE, &tv);
    reactor.time = (reactor_time) tv.tv_sec * 1000000000ULL + (reactor_time) tv.tv_nsec;;
  }

  return reactor.time;
}

void reactor_loop(void)
{
  while (pool_size(&reactor.users))
    reactor_loop_once();
}

void reactor_loop_once(void)
{
  struct io_uring_cqe *cqe;
  reactor_user *user;
  size_t i;

  if (vector_size(&reactor.next))
  {
    for (i = 0; i < vector_size(&reactor.next); i++)
    {
      user = *(reactor_user **) vector_at(&reactor.next, i);
      reactor_call(user, REACTOR_CALL, 0);
      reactor_free_user(user);
    }
    vector_clear(&reactor.next, NULL);
  }

  if (pool_size(&reactor.users))
  {
    reactor.time = 0;
    reactor_ring_update(&reactor.ring);
    while (1)
    {
      cqe = reactor_ring_cqe(&reactor.ring);
      if (!cqe)
        break;

      user = (reactor_user *) cqe->user_data;
      reactor_call(user, REACTOR_CALL, cqe->res);
      if (!(cqe->flags & IORING_CQE_F_MORE))
        reactor_free_user(user);
    }
  }
}

void reactor_call(reactor_user *user, int type, uint64_t value)
{
  user->callback((reactor_event[]){reactor_event_define(user->state, type, value)});
}

void reactor_cancel(reactor_id id, reactor_callback *callback, void *state)
{
  reactor_user *user = (reactor_user *) (id & ~0x01ULL);
  int not_iouring = id & 0x01ULL;

  *user = reactor_user_define(callback, state);
  if (!not_iouring)
    reactor_async_cancel(NULL, NULL, (uint64_t) user);
}

reactor_id reactor_async(reactor_callback *callback, void *state)
{
  reactor_async_state *async = malloc(sizeof *async);
  reactor_id id = (reactor_id) async;
  int e;

  reactor_user_construct(&async->user, callback, state);
  async->signal = eventfd(0, 0);
  async->read = reactor_read(reactor_async_return, async, async->signal, &async->counter, sizeof async->counter, 0);
  e = thrd_create(&async->id, reactor_async_call, async);
  assert(e == 0);
  e = thrd_detach(async->id);
  assert(e == 0);
  return id | 0x01ULL; /* mark as not iouring */
}

reactor_id reactor_next(reactor_callback *callback, void *state)
{
  reactor_id id = (reactor_id) reactor_alloc_user(callback, state);

  vector_push_back(&reactor.next, &id);
  return id | 0x01ULL; /* mark as not iouring */
}

reactor_id reactor_async_cancel(reactor_callback *callback, void *state, uint64_t user_data)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .addr = (uintptr_t) user_data,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_nop(reactor_callback *callback, void *state)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .opcode = IORING_OP_NOP,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_readv(reactor_callback *callback, void *state, int fd, const struct iovec *iov, int iovcnt, size_t offset)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .fd = fd,
      .opcode = IORING_OP_READV,
      .addr = (uintptr_t) iov,
      .off = offset,
      .len = iovcnt,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_writev(reactor_callback *callback, void *state, int fd, const struct iovec *iov, int iovcnt, size_t offset)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .fd = fd,
      .opcode = IORING_OP_WRITEV,
      .addr = (uintptr_t) iov,
      .off = offset,
      .len = iovcnt,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_fsync(reactor_callback *callback, void *state, int fd)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .fd = fd,
      .opcode = IORING_OP_FSYNC,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_poll_add(reactor_callback *callback, void *state, int fd, short int events)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .fd = fd,
      .opcode = IORING_OP_POLL_ADD,
      .poll_events = events,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_poll_add_multi(reactor_callback *callback, void *state, int fd, short int events)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .fd = fd,
      .opcode = IORING_OP_POLL_ADD,
      .poll_events = events,
      .len = IORING_POLL_ADD_MULTI,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_poll_update(reactor_callback *callback, void *state, reactor_id id, short int events)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .opcode = IORING_OP_POLL_REMOVE,
      .len = IORING_POLL_UPDATE_EVENTS,
      .poll_events = events,
      .addr = id,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_poll_remove(reactor_callback *callback, void *state, reactor_id id)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .opcode = IORING_OP_POLL_REMOVE,
      .addr = id,
      .user_data =(uint64_t) user
    };

  return (reactor_id) user;
}


reactor_id reactor_epoll_ctl(reactor_callback *callback, void *state, int epoll_fd, int op, int fd, struct epoll_event *event)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .opcode = IORING_OP_EPOLL_CTL,
      .fd = epoll_fd,
      .addr = (uint64_t) event,
      .len = op,
      .off = fd,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_sync_file_range(reactor_callback *callback, void *state, int fd, uint64_t offset, uint64_t nbytes, int flags)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .opcode = IORING_OP_SYNC_FILE_RANGE,
      .fd = fd,
      .off = offset,
      .len = nbytes,
      .sync_range_flags = flags,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_send(reactor_callback *callback, void *state, int fd, const void *base, size_t size, int flags)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .fd = fd,
      .opcode = IORING_OP_SEND,
      .addr = (uintptr_t) base,
      .msg_flags = flags,
      .len = size,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_recv(reactor_callback *callback, void *state, int fd, void *base, size_t size, int flags)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .fd = fd,
      .opcode = IORING_OP_RECV,
      .addr = (uintptr_t) base,
      .msg_flags = flags,
      .len = size,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_timeout(reactor_callback *callback, void *state, struct timespec *tv, int count, int flags)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .opcode = IORING_OP_TIMEOUT,
      .addr = (uintptr_t) tv,
      .len = 1,
      .off = count,
      .timeout_flags = flags,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_read(reactor_callback *callback, void *state, int fd, void *base, size_t size, size_t offset)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .fd = fd,
      .opcode = IORING_OP_READ,
      .addr = (uintptr_t) base,
      .off = offset,
      .len = size,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_write(reactor_callback *callback, void *state, int fd, const void *base, size_t size, size_t offset)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .fd = fd,
      .opcode = IORING_OP_WRITE,
      .addr = (uintptr_t) base,
      .off = offset,
      .len = size,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_connect(reactor_callback *callback, void *state, int fd, struct sockaddr *addr, socklen_t addrlen)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .fd = fd,
      .opcode = IORING_OP_CONNECT,
      .addr = (uint64_t) addr,
      .off = (uint64_t) addrlen,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_accept(reactor_callback *callback, void *state, int fd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .fd = fd,
      .opcode = IORING_OP_ACCEPT,
      .addr = (uint64_t) addr,
      .addr2 = (uint64_t) addrlen,
      .accept_flags = flags,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}

reactor_id reactor_close(reactor_callback *callback, void *state, int fd)
{
  reactor_user *user = reactor_alloc_user(callback, state);

  *reactor_ring_sqe(&reactor.ring) = (struct io_uring_sqe)
    {
      .fd = fd,
      .opcode = IORING_OP_CLOSE,
      .user_data = (uint64_t) user
    };

  return (reactor_id) user;
}
