#ifndef REACTOR_QUEUE_H_INCLUDED
#define REACTOR_QUEUE_H_INCLUDED

#include <stdlib.h>
#include "../reactor.h"

enum
{
  QUEUE_CONSUMER_POP
};

typedef struct queue          queue;
typedef struct queue_consumer queue_consumer;
typedef struct queue_producer queue_producer;

struct queue
{
  int           fd[2];
  size_t        element_size;
};

struct queue_producer
{
  queue        *queue;
  buffer        output;
  size_t        output_offset;
  buffer        output_queued;
  reactor_id    write;
};

struct queue_consumer
{
  reactor_user  user;
  queue        *queue;
  buffer        input;
  reactor_id    read;
};

void  queue_construct(queue *, size_t);
void  queue_destruct(queue *);

void  queue_producer_construct(queue_producer *);
void  queue_producer_destruct(queue_producer *);
void  queue_producer_open(queue_producer *, queue *);
void  queue_producer_push(queue_producer *, void *);
void  queue_producer_push_sync(queue_producer *, void *);
void  queue_producer_close(queue_producer *);

void  queue_consumer_construct(queue_consumer *, reactor_callback *, void *);
void  queue_consumer_destruct(queue_consumer *);
void  queue_consumer_open(queue_consumer *, queue *, size_t);
void  queue_consumer_pop(queue_consumer *);
void *queue_consumer_pop_sync(queue_consumer *);
void  queue_consumer_close(queue_consumer *);

#endif /* REACTOR_QUEUE_H_INCLUDED */
