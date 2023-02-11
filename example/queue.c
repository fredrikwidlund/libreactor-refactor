#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <reactor.h>

typedef struct object_header object_header;

struct object_header
{
  _Atomic size_t   ref;
  uint64_t         user;
  void           (*destroy)(void *);
  char             object[];
};

typedef void object;

object *object_create(void *base, size_t size, uint64_t user, void (*destroy)(void *))
{
  object_header *header;

  header = malloc(sizeof *header + size);
  *header = (object_header) {.ref = 1, .user = user, .destroy = destroy};
  if (base)
    memcpy(header->object, base, size);
  return header->object;
}

void object_hold(object *object)
{
  object_header *header = (object_header *) ((char *) object - offsetof(object_header, object));

  header->ref++;
}

void object_release(object *object)
{
  object_header *header = (object_header *) ((char *) object - offsetof(object_header, object));

  if (__sync_sub_and_fetch(&header->ref, 1) == 0)
  {
    if (header->destroy)
      header->destroy(object);
    free(header);
  }
}

uint64_t object_user(object *object)
{
  object_header *header = (object_header *) ((char *) object - offsetof(object_header, object));

  return header->user;
}

enum
{
  WRITER_ERROR,
  WRITER_FLUSH
};

typedef struct writer writer;
struct writer
{
  reactor_user user;
  int          fd;
  bool         is_socket;
  buffer       output;
  size_t       output_offset;
  buffer       output_next;
  reactor_id   id;
};

void writer_flush(writer *);
bool writer_flushed(writer *);

static void writer_callback(reactor_event *event)
{
  writer *writer = event->state;
  int result = event->data;

  writer->id = 0;
  if (result == -1)
  {
    reactor_call(&writer->user, WRITER_ERROR, -result);
    return;
  }

  writer->output_offset += result;
  if (writer->output_offset < buffer_size(&writer->output))
  {
    writer->id = reactor_send(writer_callback, writer, writer->fd,
                              buffer_at(&writer->output, writer->output_offset),
                              buffer_size(&writer->output) - writer->output_offset, 0);
    return;
  }

  writer->output_offset = 0;
  buffer_clear(&writer->output);
  writer_flush(writer);
  if (writer_flushed(writer))
    reactor_call(&writer->user, WRITER_FLUSH, 0);
}

void writer_construct(writer *writer, reactor_callback *callback, void *state)
{
  *writer = (struct writer) {.user = reactor_user_define(callback, state), .fd = -1};
  buffer_construct(&writer->output);
  buffer_construct(&writer->output_next);
}

void writer_destruct(writer *writer)
{
  assert(writer->id == 0);
  buffer_destruct(&writer->output);
  buffer_destruct(&writer->output_next);
}

void writer_open_socket(writer *writer, int fd)
{
  assert(writer->fd == -1);
  writer->fd = fd;
  writer->is_socket = true;
}

void writer_open(writer *writer, int fd)
{
  struct stat st;

  assert(fstat(fd, &st) == 0);
  if ((st.st_mode & S_IFMT) == S_IFSOCK)
    writer_open_socket(writer, fd);
  else
    assert(0);
}

bool writer_flushed(writer *writer)
{
  return buffer_empty(&writer->output) && buffer_empty(&writer->output_next);
}

void writer_write(writer *writer, data data)
{
  memcpy(buffer_allocate(&writer->output_next, data_size(data)), data_base(data), data_size(data));
}

void writer_flush(writer *writer)
{
  buffer buffer;

  if (writer->id)
    return;
  if (buffer_empty(&writer->output_next))
    return;

  buffer = writer->output;
  writer->output = writer->output_next;
  writer->output_next = buffer;
  writer->id = reactor_send(writer_callback, writer, writer->fd,
                            buffer_base(&writer->output), buffer_size(&writer->output), 0);
}

typedef struct queue queue;

struct queue
{
  int            head;
  int            tail;
  size_t         element_size;
  _Atomic size_t ref;
};

void queue_construct(queue *queue, size_t element_size)
{
  int e, fd[2];

  e = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
  assert(e == 0);
  *queue = (struct queue) {.head = fd[0], .tail = fd[1], .element_size = element_size};
}

void queue_destruct(queue *queue)
{
  assert(queue->ref == 0);
  (void) reactor_close(NULL, NULL, queue->head);
  (void) reactor_close(NULL, NULL, queue->tail);
}

typedef struct queue_producer queue_producer;

struct queue_producer
{
  reactor_user  user;
  queue        *queue;
  writer        writer;
};

void queue_producer_construct(queue_producer *producer, reactor_callback *callback, void *state)
{
  *producer = (queue_producer) {.user = reactor_user_define(callback, state)};
  writer_construct(&producer->writer, NULL, NULL);
}

void queue_producer_destruct(queue_producer *producer)
{
  producer->queue->ref--;
  writer_destruct(&producer->writer);
}

void queue_producer_open(queue_producer *producer, queue *queue)
{
  producer->queue = queue;
  producer->queue->ref++;
  writer_open(&producer->writer, producer->queue->head);
}

void queue_producer_push(queue_producer *producer, void *element)
{
  writer_write(&producer->writer, data_define(element, producer->queue->element_size));
  writer_flush(&producer->writer);
}


enum
{
  READER_ERROR,
  READER_CLOSE,
  READER_INPUT
};

typedef struct reader reader;
struct reader
{
  reactor_user user;
  int          fd;
  bool         is_socket;
  buffer       input;
  reactor_id   id;
};

static void reader_callback(reactor_event *event)
{
  reader *reader = event->state;
  int result = event->data;
  data data;

  reader->id = 0;
  if (result == 0)
  {
    reactor_call(&reader->user, READER_CLOSE, 0);
    return;
  }

  if (result < 0)
  {
    reactor_call(&reader->user, READER_ERROR, -result);
    return;
  }

  data = data_define(buffer_end(&reader->input), result);
  buffer_resize(&reader->input, buffer_size(&reader->input) + result);
  reactor_call(&reader->user, READER_INPUT, (uint64_t) &data);
}

void reader_construct(reader *reader, reactor_callback *callback, void *state)
{
  *reader = (struct reader) {.user = reactor_user_define(callback, state), .fd = -1};
  buffer_construct(&reader->input);
}

void reader_destruct(reader *reader)
{
  assert(reader->id == 0);
  buffer_destruct(&reader->input);
}

void reader_open_socket(reader *reader, int fd)
{
  assert(reader->fd == -1);
  reader->fd = fd;
  reader->is_socket = true;
}

void reader_open(reader *reader, int fd)
{
  struct stat st;

  assert(fstat(fd, &st) == 0);
  if ((st.st_mode & S_IFMT) == S_IFSOCK)
    reader_open_socket(reader, fd);
  else
    assert(0);
}

bool reader_opened(reader *reader)
{
  return reader->fd >= 0;
}

void reader_close(reader *reader)
{
  assert(reader->id == 0);
  assert(reader_opened(reader));
  reader->fd = -1;
}

data reader_data(reader *reader)
{
  return buffer_data(&reader->input);
}

void reader_read(reader *reader, size_t size)
{
  assert(reader->id == 0);
  buffer_reserve(&reader->input, buffer_size(&reader->input) + size);
  reader->id = reactor_recv(reader_callback, reader, reader->fd, buffer_end(&reader->input), size, 0);
}

void reader_consume(reader *reader, size_t size)
{
  buffer_erase(&reader->input, 0, size);
}

enum
{
  QUEUE_CONSUMER_POP
};

typedef struct queue_consumer queue_consumer;

struct queue_consumer
{
  reactor_user  user;
  queue        *queue;
  size_t        batch;
  reader        reader;
};

static void queue_consumer_callback(reactor_event *event)
{
  queue_consumer *consumer = event->state;
  size_t size = consumer->queue->element_size;
  data data;

  assert(event->type == READER_INPUT);
  data = reader_data(&consumer->reader);
  while (data_size(data) >= size)
  {
    reactor_call(&consumer->user, QUEUE_CONSUMER_POP, (uint64_t) data_base(data));
    // XXX queue could be destructed, guard needed
    if (!reader_opened(&consumer->reader))
      return;
    data = data_offset(data, size);
  }
  reader_consume(&consumer->reader, data_size(reader_data(&consumer->reader)) - data_size(data));
  reader_read(&consumer->reader, consumer->queue->element_size * consumer->batch);
}

void queue_consumer_construct(queue_consumer *consumer, reactor_callback *callback, void *state)
{
  *consumer = (queue_consumer) {.user = reactor_user_define(callback, state), .batch = 1};
  reader_construct(&consumer->reader, queue_consumer_callback, consumer);
}

void queue_consumer_destruct(queue_consumer *consumer)
{
  consumer->queue->ref--;
  reader_destruct(&consumer->reader);
}

void queue_consumer_open(queue_consumer *consumer, queue *queue)
{
  consumer->queue = queue;
  consumer->queue->ref++;
  reader_open(&consumer->reader, consumer->queue->tail);
  reader_read(&consumer->reader, consumer->queue->element_size * consumer->batch);
}

void queue_consumer_close(queue_consumer *consumer)
{
  reader_close(&consumer->reader);
}

void queue_consumer_batch(queue_consumer *consumer, size_t batch)
{
  consumer->batch = batch;
}

static void sender(reactor_event *event)
{
  queue_producer producer;
  object *object;
  int i;

  if (event->type == REACTOR_RETURN)
    return;

  reactor_construct();
  queue_producer_construct(&producer, NULL, NULL);
  queue_producer_open(&producer, event->state);
  for (i = 0; i < 100000; i++)
  {
    object = object_create((uint64_t[]){4711}, sizeof (uint64_t), 42, NULL);
    queue_producer_push(&producer, &object);
  }
  reactor_loop();
  queue_producer_destruct(&producer);
  reactor_destruct();
}

static void receiver(reactor_event *event)
{
  queue_consumer *consumer = event->state;
  object *object;
  static int count = 0;

  assert(event->type == QUEUE_CONSUMER_POP);
  assert(event->data);
  object = *(struct object **) event->data;
  assert(object_user(object) == 42);
  assert(*(uint64_t *)object == 4711);
  object_release(object);
  count++;
  if (count == 10000000)
    queue_consumer_close(consumer);
}

int main()
{
  queue queue;
  queue_consumer consumer;
  int i;

  reactor_construct();
  queue_construct(&queue, sizeof (void *));
  queue_consumer_construct(&consumer, receiver, &consumer);
  queue_consumer_open(&consumer, &queue);
  queue_consumer_batch(&consumer, 1024);
  for (i = 0; i < 100; i++)
    reactor_async(sender, &queue);
  reactor_loop();
  queue_consumer_destruct(&consumer);
  queue_destruct(&queue);
  reactor_destruct();
}
