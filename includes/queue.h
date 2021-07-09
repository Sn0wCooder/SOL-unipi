#ifndef QUEUE_H_
#define QUEUE_H_

#include <pthread.h>

typedef struct _Node {
    void*           data;
    struct _Node*    next;
} Node;

/**
*
*/
typedef struct _Queue {
    Node*             head;
    Node*             tail;
    unsigned long       len;
} Queue;

int push(Queue **q, void* el);
Queue* initQueue();
void* pop(Queue **q);
void* pop2(Queue **q);
void* returnFirstEl(Queue *q);
int removeFromQueue(Queue **q, Node* toDelete);

#endif
