#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "queue.h"

#ifndef QUEUE_H_
#define QUEUE_H_

Queue initQueue() { //inizializza una coda vuota
  Queue q = malloc(sizeof(Queue));
  //Node n = malloc(sizeof(Node));
  q->head = NULL;
  q->tail = NULL;
  q->len = 0;
  return q;
}

void push(Queue **q, void* el) { //inserimento in coda in una FIFO
  Node n = malloc(sizeof(Node));
  n->data = el;
  n->next = NULL;
  //inserimento in coda
  if(*q->head == NULL) { //inserimento in coda vuota
    *q->head = n;
    *q->tail = n;
    *q->len = 1;
  } else { //inserimento in coda
    (*q->tail)->next = n;
    *q->tail = n;
    *q->len = *q->len + 1;
  }
}

#endif
