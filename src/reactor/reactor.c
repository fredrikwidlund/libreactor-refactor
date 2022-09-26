#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/socket.h>
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

/* reactor */

struct reactor
{
  size_t               ref;
  size_t               users;
  reactor_ring         ring;
};

static __thread struct reactor reactor = {0};

static void reactor_default_callback(reactor_event event)
{
  (void) event;
}

static reactor_user *reactor_user_create(reactor_callback *callback, void *state)
{
  reactor_user *user = malloc(sizeof *user);

  *user = (reactor_user) {.callback = callback ? callback : reactor_default_callback, .state = state};
  reactor.users++;
  return user;
}

static void reactor_user_destroy(reactor_user *user)
{
  free(user);
  reactor.users--;
}

__attribute__ ((constructor))
void reactor_construct(void)
{
  if (!reactor.ref)
  {
    reactor_ring_construct(&reactor.ring, REACTOR_RING_SIZE);
  }
  reactor.ref++;
}

__attribute__ ((destructor))
void reactor_destruct(void)
{
  reactor.ref--;
  if (!reactor.ref)
  {
    reactor_ring_destruct(&reactor.ring);
  }
}

void reactor_loop(void)
{
  while (reactor.users)
    reactor_loop_once();
}

void reactor_loop_once(void)
{
  struct io_uring_cqe *cqe;
  reactor_user *user;

  if (!reactor.users)
    return;

  reactor_ring_update(&reactor.ring);
  while (1)
  {
    cqe = reactor_ring_cqe(&reactor.ring);
    if (!cqe)
      break;

    user = (reactor_user *) cqe->user_data;
    user->callback((reactor_event[]){{.state = user->state, 0, cqe->res}});
    reactor_user_destroy(user);
    // XXX add if (!(cqe->flags & IORING_CQE_F_MORE))  when IORING_POLL_ADD_MULTI is implemented
  }
}

reactor_id reactor_recv(reactor_callback *callback, void *state, int fd, void *base, size_t size, int flags)
{
  reactor_user *user = reactor_user_create(callback, state);

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

reactor_id reactor_read(reactor_callback *callback, void *state, int fd, void *base, size_t size, size_t offset)
{
  reactor_user *user = reactor_user_create(callback, state);

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

reactor_id reactor_send(reactor_callback *callback, void *state, int fd, const void *base, size_t size, int flags)
{
  reactor_user *user = reactor_user_create(callback, state);

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

reactor_id reactor_timeout(reactor_callback *callback, void *state, struct timespec *tv, int count, int flags)
{
  reactor_user *user = reactor_user_create(callback, state);

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

reactor_id reactor_accept(reactor_callback *callback, void *state, int fd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
  reactor_user *user = reactor_user_create(callback, state);

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
