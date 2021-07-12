#include <pthread.h>

typedef struct _Node {
  void* data;
  struct _Node* next;
} Node; //nodo generico di una coda

typedef struct _Queue {
  Node* head;
  Node* tail;
  unsigned long len;
} Queue; //coda generica

int push(Queue **q, void* el); //inserimento in coda nella coda
int pushTesta(Queue **q, void* el); //inserimento in testa nella coda
Queue* initQueue(); //inizializza una coda
void* pop(Queue **q); //ritorna e rimuove il primo elemento dalla coda
void* pop2(Queue **q); //ritorna e rimuove il secondo elemento dalla coda
void* returnFirstEl(Queue *q); //ritorna senza rimuoverlo il primo elemento dalla coda
int removeFromQueue(Queue **q, Node* toDelete); //rimuove dalla coda il nodo toDelete
