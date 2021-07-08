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
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
} Queue;

int push(Queue **q, void* el);
Queue* initQueue();
void* pop(Queue **q);
void* pop2(Queue **q);
void* returnFirstEl(Queue *q);
int removeFromQueue(Queue **q, Node* toDelete);

#endif
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
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
} Queue;

int push(Queue **q, void* el);
Queue* initQueue();
void* pop(Queue **q);
void* pop2(Queue **q);
void* returnFirstEl(Queue *q);
int removeFromQueue(Queue **q, Node* toDelete);

#endif
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
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
} Queue;

int push(Queue **q, void* el);
Queue* initQueue();
void* pop(Queue **q);
void* pop2(Queue **q);
void* returnFirstEl(Queue *q);
int removeFromQueue(Queue **q, Node* toDelete);

#endif
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
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
} Queue;

int push(Queue **q, void* el);
Queue* initQueue();
void* pop(Queue **q);
void* pop2(Queue **q);
void* returnFirstEl(Queue *q);
int removeFromQueue(Queue **q, Node* toDelete);

#endif
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
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
} Queue;

int push(Queue **q, void* el);
Queue* initQueue();
void* pop(Queue **q);
void* pop2(Queue **q);
void* returnFirstEl(Queue *q);
int removeFromQueue(Queue **q, Node* toDelete);

#endif
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
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
} Queue;

int push(Queue **q, void* el);
Queue* initQueue();
void* pop(Queue **q);
void* pop2(Queue **q);
void* returnFirstEl(Queue *q);
int removeFromQueue(Queue **q, Node* toDelete);

#endif
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
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
} Queue;

int push(Queue **q, void* el);
Queue* initQueue();
void* pop(Queue **q);
void* pop2(Queue **q);
void* returnFirstEl(Queue *q);
int removeFromQueue(Queue **q, Node* toDelete);

#endif
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
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
} Queue;

int push(Queue **q, void* el);
Queue* initQueue();
void* pop(Queue **q);
void* pop2(Queue **q);
void* returnFirstEl(Queue *q);
int removeFromQueue(Queue **q, Node* toDelete);

#endif
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
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
} Queue;

int push(Queue **q, void* el);
Queue* initQueue();
void* pop(Queue **q);
void* pop2(Queue **q);
void* returnFirstEl(Queue *q);
int removeFromQueue(Queue **q, Node* toDelete);

#endif
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
    pthread_mutex_t     lock;
    pthread_cond_t      cond;
} Queue;

int push(Queue **q, void* el);
Queue* initQueue();
void* pop(Queue **q);
void* pop2(Queue **q);
void* returnFirstEl(Queue *q);
int removeFromQueue(Queue **q, Node* toDelete);

#endif
