#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "queue.h"
#include "util.h"

Queue* initQueue() { //inizializza una coda vuota
  Queue *q;
  ec_null((q = malloc(sizeof(Queue))), "malloc");
  q->head = NULL;
  q->tail = NULL;
  q->len = 0;
  return q;
}

int push(Queue **q, void* el) { //inserimento in coda in una FIFO
  Node *n;
  ec_null((n = malloc(sizeof(Node))), "malloc");
  if(*q == NULL)
    return -1;
  n->data = el;
  n->next = NULL;
  //inserimento in coda
  if((*q)->len == 0) { //inserimento in coda vuota
    (*q)->head = n;
    (*q)->tail = n;
    (*q)->len = 1;
  } else { //inserimento in coda (lista non vuota)
    ((*q)->tail)->next = n;
    (*q)->tail = n;
    (*q)->len++;
  }
  return 0;
}

int pushTesta(Queue **q, void* el) { //inserimento in testa in una FIFO
  Node *n;
  ec_null((n = malloc(sizeof(Node))), "malloc");
  if(*q == NULL)
    return -1;
  n->data = el;
  n->next = (*q)->head;
  if((*q)->len == 0) { //inserimento in coda vuota
    (*q)->head = n;
    (*q)->tail = n;
    (*q)->len = 1;
  } else { //inserimento in testa (lista non vuota)
    (*q)->head = n;
    (*q)->len++;
  }
  return 0;
}

void* returnFirstEl(Queue *q) { //ritorna il primo elemento dalla coda q
  if(q->head == NULL) { //la coda è vuota
    return NULL;
  } else { //lista piena, ritorna il primo elemento
    void *ret = (q->head)->data;
    return ret;
  }
}

void* pop(Queue **q) { //restituisce la testa e la rimuove dalla queue
  if((*q)->head == NULL) { //la coda è vuota
    return NULL;
  } else { //coda piena, restituisce il primo elemento
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
  if((*q)->head == NULL) { //la coda è vuota
    return NULL;
  } else { //coda non vuota, restituisce il secondo elemento
    Node* firstel = (*q)->head;
    Node *secondel = firstel->next;
    void* ret = secondel->data;
    firstel->next = secondel->next;
    (*q)->len--;
    free(secondel);
    return ret;
  }
}

int removeFromQueue(Queue **q, Node* toDelete) { //rimuove dalla coda il nodo toDelete
  int ok = -1; //inizialmente errore
  Node* tmp = (*q)->head;
  Node* tmp_prec = NULL; //precedente
  while(tmp != NULL) {
    if(toDelete == tmp) { //è proprio il nodo da cancellare
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
      tmp = NULL;
      ok = 0; //successo, cancellato
    }
    if(tmp != NULL) {
      tmp_prec = tmp;
      tmp = tmp->next;
    }
  }
  return ok;
}
