#pragma once
/* This queue is used to store node indices, for implementing BFS.  It uses a
 * circular buffer that grows dynamically. It should not be used for extremely
 * large use cases.
 */

typedef struct queue {
  /* Size is the allocated room, count is the number of elements */
  size_t size, count;
  size_t *buf;
  size_t head, tail;
} queue;

static inline char queue_empty(queue *q) {
  if(!q) return 1;
  if(!q->count) return 1;
  return 0;
}

/* Initializes a queue. Call this first. */
static inline queue *queue_init() {
  queue *q;

  q = (queue *) malloc(sizeof(queue));
  q->size = 16;
  q->count = 0;
  q->buf = (size_t *) malloc(sizeof(size_t) * q->size);
  q->head = 0;
  q->tail = 0;

  return q;
}

/* Pushes onto the queue */
static inline void queue_push(queue *q, size_t val) {
  if(!q) return;
  if(q->count == q->size) {
    /* Resize the buffer */
    q->size *= 2;
    q->buf = (size_t *) realloc(q->buf, sizeof(size_t) * q->size);
  }
  /* Insert the value and increase the tail */
  q->buf[q->tail] = val;
  q->tail = (q->tail + 1) % q->size;
  q->count++;
}

/* Pops from the queue */
static inline size_t queue_pop(queue *q) {
  size_t val;

  if(!q) return SIZE_MAX;
  if(q->count == 0) return SIZE_MAX;

  val = q->buf[q->head];
  q->head = (q->head + 1) % q->size;
  q->count--;

  return val;
}

/* Frees up a queue */
static inline void queue_free(queue *q) {
  if(!q) return;
  free(q->buf);
  free(q);
}
