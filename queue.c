#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "queue.h"

//#ifndef QUEUE_H_
//#define QUEUE_H_

Queue* initQueue() { //inizializza una coda vuota
  Queue *q = malloc(sizeof(Queue));
  //Node n = malloc(sizeof(Node));
  q->head = NULL;
  q->tail = NULL;
  q->len = 0;
  return q;
}

void push(Queue **q, void* el) { //inserimento in coda in una FIFO
  Node *n = malloc(sizeof(Node));
  n->data = el;
  n->next = NULL;
  fprintf(stderr, "ciao1\n");
  //inserimento in coda
  if((*q)->len == 0) { //inserimento in coda vuota
    fprintf(stderr, "coda vuota\n");

    (*q)->head = n;
    (*q)->tail = n;
    (*q)->len = 1;
  } else { //inserimento in coda (lista non vuota)

    //fprintf(stderr, "coda non vuota, q->head %ld, %ld\n", (*q)->head, (((*q)->head)->data));
    ((*q)->tail)->next = n;
    (*q)->tail = n;
    (*q)->len++; // = *q->len + 1;
  }
}

void* returnFirstEl(Queue *q) {
  if(q->head == NULL) { //la lista è già vuota
    //fprintf(stderr, "lista vuota");
    return NULL;
  } else {
    void *ret = (q->head)->data;
    return ret;
  }
}

void* pop(Queue **q) { //restituisce la testa e la rimuove dalla queue
  if((*q)->head == NULL) { //la lista è già vuota
    //fprintf(stderr, "lista vuota");
    return NULL;
  } else {
    void *ret = ((*q)->head)->data;
    Node* tmp = (*q)->head;
    (*q)->head = ((*q)->head)->next;
    (*q)->len--;
    if((*q)->head == NULL) //la lista conteneva un solo elemento
      (*q)->tail = NULL;

    free(tmp);
    return ret;
  }
}

void* pop2(Queue **q) { //restituisce il secondo elemento e lo rimuove dalla queue
  if((*q)->head == NULL) { //la lista è già vuota
    //fprintf(stderr, "lista vuota");
    return NULL;
  } else {
    Node* firstel = (*q)->head;
    Node *secondel = firstel->next;
    void* ret = secondel->data;
    firstel->next = secondel->next;
    (*q)->len--;
    free(secondel);
    return ret;
  }
}

int removeFromQueue(Queue **q, Node* toDelete) {
  int ok = -1; //inizialmente errore
  Node* tmp = (*q)->head;
  Node* tmp_prec = NULL;
  while(tmp != NULL) {
    //fprintf(stdout, "nomefile %s length %ld\n", no->nome, no->length);
    if(toDelete == tmp) { //nodo da cancellare
      //pthread_mutex_unlock(&mutexQueueFiles);
      ((*q)->len)--;
      if(tmp_prec == NULL) { //cancellazione in testa
        (*q)->head = tmp->next;
        if((*q)->len == 0) //c'era un solo elemento
          (*q)->tail = NULL;
      } else { //cancellazione in mezzo
        tmp_prec->next = tmp->next;
        if(tmp->next == NULL) //ultimo elemento nella coda
          (*q)->tail = tmp_prec;
      }
      free(tmp);
      ok = 0; //successo, cancellato
    }
    tmp_prec = tmp;
    tmp = tmp->next;
  }
  return ok;
}

//#endif
