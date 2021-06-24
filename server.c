#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <getopt.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <sys/un.h>
#include <ctype.h>

#include "queue.h"

//"nomeInfo valore"
//"spazio 1000"
//"numeroFIle 100"
#define MAXBUFFER 1000
#define MAXSTRING 100
#define CONFIGFILE "config.txt"
#define SPAZIO "spazio"
#define NUMEROFILE "numeroFile"
#define SOC "sockName"
#define WORK "numWorkers"
#define MAXBACKLOG 32

int spazio = 0;
int numeroFile = 0;
int numWorkers = 0;
char* SockName;
Queue *queueClient;

pthread_mutex_t *mutexQueueClient;

typedef struct _file {
  char* nome;
  long size;
  char* buffer; //contenuto
  long length; //per fare la read e la write, meglio se memorizzato
} File;

static inline int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
  if ((r=read((int)fd ,bufptr,left)) == -1) {
      if (errno == EINTR) continue;
      return -1;
  }
  if (r == 0) return 0;   // EOF
        left    -= r;
  bufptr  += r;
    }
    return size;
}

/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
static inline int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
  if ((r=write((int)fd ,bufptr,left)) == -1) {
      if (errno == EINTR) continue;
      return -1;
  }
  if (r == 0) return 0;
        left    -= r;
  bufptr  += r;
    }
    return 1;
}


typedef struct msg {
    int len;
    char *str;
} msg_t;

void cleanup() {
  unlink(SockName);
}

int isNumber (char* s) {
  int ok = 1;
  int len = strlen(s);
  int i = 0;
  while(ok && i < len) {
    if(!isdigit(s[i]))
      ok = 0;
    i++;
  }
  return ok;
}

int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
	if (FD_ISSET(i, &set)) return i;
    //assert(1==0);
    return -1;
}



void parser(void) {
  char* a = NULL;
  int i;
  char* save;
  char* token;
  //int spazio = 0;
  //int numeroFile = 0;
  //int numWorkers = 0;
  //char* SockName;
  char* buffer = malloc(sizeof(char) * MAXBUFFER);
  FILE* p;
  //fprintf(stderr,"NON abbiamo aperto il file\n");
  if((p = fopen(CONFIGFILE, "r")) == NULL) {
        //gestione dell'errore
    perror("fopen");
  }
  //fprintf(stderr, "abbiamo aperto il file\n");
  while(fgets(buffer, MAXBUFFER, p)) {
    //fprintf(stderr, "ecco il buffer: %s\n", buffer);
        //ora facciamo il parser della singola riga

            //tokenizzo la stringa e vedo il numero di virgole
    save = NULL;
    token = strtok_r(buffer, " ",  &save);
    char** tmp = malloc(sizeof(char*) * 2);
    i = 0;
    while(token) {
      //fprintf(stderr, "sto processando %s\n", token);
      tmp[i] = malloc(sizeof(char) * MAXSTRING);
      strncpy(tmp[i], token, strlen(token) - i); //se è il secondo elemento (i = i) non deve prendere il newline finale,
      token = strtok_r(NULL, " ", &save);
      i++;
    }

    //fprintf(stderr, "Array: %s %s \n", tmp[0], tmp[1]);

    if(strcmp(tmp[0], SPAZIO) == 0) {
                //le due stringhe sono uguali
                //fprintf(stderr,"questo è il primo arg %s\n",tmp[1]);
      //fprintf(stderr,"questo è il risultato dello spazio %d\n", isNumber(tmp[1]));
      if(isNumber(tmp[1])) {
                    //fprintf(stderr,"questo è a %s\n",a);
        spazio = atoi(tmp[1]);
                    //fprintf(stderr,"questo è il risultato %d\n",spazio);
      } else {
        perror("errato config.txt (isNumber)");
        exit(EXIT_FAILURE);
        //fprintf(stderr, "ERRORE %s non è un numero\n", tmp[1]);
      }
    }
    if(strcmp(tmp[0], NUMEROFILE) == 0) {
      if(isNumber(tmp[1])) {
        numeroFile = atoi(tmp[1]);
      } else {
        perror("errato config.txt (isNumber)");
        exit(EXIT_FAILURE);
      }
    }
    if(strcmp(tmp[0], SOC) == 0) {
      SockName = malloc(sizeof(char) * strlen(tmp[1]));
      strncpy(SockName, tmp[1], strlen(tmp[1]));
      //numeroFile = atoi(tmp[1]);
    }
    if(strcmp(tmp[0], WORK) == 0) {
      if(isNumber(tmp[1])) {
        numWorkers = atoi(tmp[1]);
      } else {
        perror("errato config.txt (isNumber)");
        exit(EXIT_FAILURE);
      }
    }

  free(tmp[0]);
  free(tmp[1]);
  free(tmp);
  }
  fclose(p);



  free(buffer);


  //fprintf(stderr,"abbiamo chiuso il file\n");
  fprintf(stderr,"spazio: %d\n", spazio);
  fprintf(stderr,"numeroFile: %d\n", numeroFile);
  fprintf(stderr,"SockName: %s\n", SockName);
  fprintf(stderr,"numWorkers: %d\n", numWorkers);
}

static void* threadF(void* arg) {
  fprintf(stderr, "ciao\n");
  while(1) {
    pthread_mutex_lock(mutexQueueClient);
    void* tmp = pop(&queueClient);
    pthread_mutex_unlock(mutexQueueClient);

    if(tmp == NULL)
      continue;
    fprintf(stderr, "ciaoyguyj\n");

    long connfd = (long)tmp;

    //char* letturaSocket;



    msg_t str;
    if (readn(connfd, &str.len, sizeof(int))<=0) return NULL;
    str.str = calloc((str.len), sizeof(char));
    if (!str.str) {
	perror("calloc");
	fprintf(stderr, "Memoria esaurita....\n");
	return NULL;
    }
    if (readn(connfd, str.str, str.len*sizeof(char))<=0) return NULL;
    //toup(str.str);
    if (writen(connfd, &str.len, sizeof(int))<=0) { free(str.str); return NULL;}
    if (writen(connfd, str.str, str.len*sizeof(char))<=0) { free(str.str); return NULL;}
    free(str.str);

  }
  return NULL;
}

int main(int argc, char* argv[]) {
  parser();
  cleanup();
  atexit(cleanup);
  queueClient = initQueue(); //coda dei file descriptor dei client che provano a connettersi



  pthread_t *t = malloc(sizeof(pthread_t) * numWorkers); //array dei thread
  for(int i = 0; i < numWorkers; i++) {
    pthread_create(&t[i], NULL, &threadF, NULL);
    //sleep(1);
  }


  int listenfd;

  //SYSCALL_EXIT("socket", listenfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
  listenfd = socket(AF_UNIX, SOCK_STREAM, 0);

  struct sockaddr_un serv_addr;
  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sun_family = AF_UNIX;
  strncpy(serv_addr.sun_path, SockName, strlen(SockName)+1);

  int notused;
  //SYSCALL_EXIT("bind", notused, bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)), "bind", "");
  notused = bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr));
  //SYSCALL_EXIT("listen", notused, listen(listenfd, MAXBACKLOG), "listen", "");
  notused = listen(listenfd, MAXBACKLOG);

  fd_set set, tmpset;
  // azzero sia il master set che il set temporaneo usato per la select
  FD_ZERO(&set);
  FD_ZERO(&tmpset);

  // aggiungo il listener fd al master set
  FD_SET(listenfd, &set);

  // tengo traccia del file descriptor con id piu' grande
  int fdmax = listenfd;
  //fprintf(stderr, "ciao\n");
  for(;;) {
// copio il set nella variabile temporanea per la select
    tmpset = set;
    if (select(fdmax+1, &tmpset, NULL, NULL, NULL) == -1) {
      perror("select");
      return -1;
    }
// cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
    for(int i=0; i <= fdmax; i++) {
      fprintf(stderr, "ciao\n");
      if (FD_ISSET(i, &tmpset)) {
        long connfd;
        if (i == listenfd) { // e' una nuova richiesta di connessione
          //SYSCALL_EXIT("accept", connfd, accept(listenfd, (struct sockaddr*)NULL ,NULL), "accept", "");
          connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL);
          FD_SET(connfd, &set);  // aggiungo il descrittore al master set
          if(connfd > fdmax)
            fdmax = connfd;  // ricalcolo il massimo
          continue;
        }
        connfd = i;  // e' una nuova richiesta da un client già connesso

  // eseguo il comando e se c'e' un errore lo tolgo dal master set
        //if (cmd(connfd) < 0) {
        /*if (-1 < 0) {
          close(connfd);
          FD_CLR(connfd, &set);
    // controllo se deve aggiornare il massimo
          if (connfd == fdmax)
            fdmax = updatemax(set, fdmax);
        }*/
        //fprintf(stderr, "ciao1 %ld\n", connfd);
        push(&queueClient, &connfd);
        //fprintf(stderr, "ciao2\n");
        //printf("inserito\n");
      }
    }
  }
}
