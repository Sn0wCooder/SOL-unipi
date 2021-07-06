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
#include <pthread.h>
#include <signal.h>
#include <libgen.h>

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
Queue *queueClient; //coda dei comandi gestiti dai thread
Queue *queueFiles; //coda dei file memorizzati

int spazioOccupato = 0;

int rsigint;
int rsighup;
int* psegnali;

fd_set set;
int **p;

static pthread_mutex_t mutexQueueClient = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutexQueueFiles = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condQueueClient = PTHREAD_COND_INITIALIZER;

typedef struct _comandoclient {
  char comando;
  char* parametro;
  long connfd;
} ComandoClient;

typedef struct _file {
  char* nome;
  //long size;
  char* buffer; //contenuto
  long length; //per fare la read e la write, meglio se memorizzato
  int is_locked;
  pthread_mutex_t lock;
} fileRAM;

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
  fprintf(stderr, "sono nell'updatemax\n");
  for(int i=(fdmax-1);i>=0;--i)
	 if (FD_ISSET(i, &set))
    return i;
    //assert(1==0);
    return -1;
}

int areThereConnections(fd_set set, int fdmax) {
  for(int i=(fdmax-1);i>=0;--i) {
    //fprintf(stderr, "indice i %d: %d\n", i, FD_ISSET(i, &set));
    for(int j = 0; j < numWorkers; j++) {
      if(i != p[j][0] && FD_ISSET(i, &set)) {
        return 1;
      }
    }
  }
  return 0;
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

  if(spazio <= 0 || numeroFile <= 0 || SockName == NULL || numWorkers <= 0) {
    fprintf(stderr, "config.txt errato\n");
    exit(EXIT_FAILURE);
  }
  //fprintf(stderr,"abbiamo chiuso il file\n");
  fprintf(stderr,"spazio: %d\n", spazio);
  fprintf(stderr,"numeroFile: %d\n", numeroFile);
  fprintf(stderr,"SockName: %s\n", SockName);
  fprintf(stderr,"numWorkers: %d\n", numWorkers);
}

void printQueueFiles(Queue *q) {
  Node* tmp = q->head;
  fileRAM *no = NULL;
  while(tmp != NULL) {
    no = tmp->data;
    fprintf(stdout, "nomefile %s length %ld\n", no->nome, no->length);
    tmp = tmp->next;
  }
}

Node* fileExistsInServer(Queue *q, char* nomefile) {
  //pthread_mutex_lock(&mutexQueueFiles);
  Node* tmp = q->head;
  fileRAM *no = NULL;
  while(tmp != NULL) {
    no = tmp->data;
    //fprintf(stdout, "nomefile %s length %ld\n", no->nome, no->length);
    if(strcmp(basename(nomefile), no->nome) == 0) {
      //pthread_mutex_unlock(&mutexQueueFiles);
      return tmp;
    }
    tmp = tmp->next;
  }
  //pthread_mutex_unlock(&mutexQueueFiles);
  return NULL;
}

static void* threadF(void* arg) {
  int numthread = *(int*)arg;
  fprintf(stderr, "num thread %d\n", numthread);
  //fprintf(stderr, "ciao\n");
  while(1) {
    pthread_mutex_lock(&mutexQueueClient);
    while(queueClient->len == 0) {
      pthread_cond_wait(&condQueueClient, &mutexQueueClient);
      fprintf(stderr, "sono sveglio (num thread %d)!\n", numthread);
    }
    ComandoClient* tmp = pop(&queueClient);
    //fprintf(stderr, "lunghezza lista %lu\n", queueClient->len);
    pthread_mutex_unlock(&mutexQueueClient);


    //fprintf(stderr, "ciaoyguyj\n");
    long connfd = tmp->connfd;
    char comando = tmp->comando;
    char* parametro = tmp->parametro;

    fprintf(stderr, "connfd %ld, comando %c, parametro %s\n", connfd, comando, parametro);
    //print
    //char* risposta = malloc(sizeof(char) * MAXBUFFER);
    //int lenRisposta;
    int notused;

    //POSSIBILI COMANDI PASSATI
    switch(comando) {
      case 'W': { //richiesta di scrittura
        //fprintf(stderr, "sono nello writch\n");
        pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro);
        pthread_mutex_unlock(&mutexQueueFiles);

        int ris = 0;
        if(esiste == NULL) { //errore: il file non esiste
          ris = -1;
          if (writen(connfd, &ris, sizeof(int))<=0) { perror("c"); }
        } else { //file esiste
          fileRAM *newfile = esiste->data;
          pthread_mutex_lock(&newfile->lock);
          if(newfile->is_locked != connfd) { //file eistse ma non aperto dal client
            ris = -1;
          }
          if (writen(connfd, &ris, sizeof(int))<=0) { perror("c"); }

          if(ris != -1) {
            int lentmp;
            notused = readn(connfd, &lentmp, sizeof(int));
            int cista = 1; //ci sta nel server fino a prova contraria
            if(lentmp > spazio || lentmp + newfile->length > spazio) { //non lo memorizza proprio
              //il secondo controllo dopo l'or è per vedere se l'append di un file non ci sta nel server
              fprintf(stderr, "Il file %s è troppo grande (%ld) e non sta materialmente nel server (capienza massima %d)\n", newfile->nome, newfile->length, spazio);
                //vanno fatte delle FREE
                //vado a scrivere nel socket che il file non ci sta
              if(lentmp > spazio) //se il file non esiste ancora (ha size null)
                removeFromQueue(&queueFiles, esiste);
              cista = 0;
            }
            if (writen(connfd, &cista, sizeof(int))<=0) { perror("c"); }
            if(cista) {
              //fprintf(stderr, "UEEEEEEE\n");
              fileRAM *fileramtmptrash;
              pthread_mutex_lock(&mutexQueueFiles);
              while(spazioOccupato + lentmp > spazio) { //deve iniziare ad espellere file
                fprintf(stderr, "il server è pieno (di spazio)\n");
                fileRAM *firstel = returnFirstEl(queueFiles);
                if(newfile != firstel) { //non rimuove il primo elemento perchè è proprio quello che dobbiamo inserire
                  fileramtmptrash = pop(&queueFiles);
                } else { //rimuove il secondo elemento
                  fileramtmptrash = pop2(&queueFiles);
                }
                fprintf(stderr, "Sto espellendo il file %s dal server per far spazio al file %s\n", fileramtmptrash->nome, parametro);
                spazioOccupato-=fileramtmptrash->length;
                  //vanno fatte FREE
              }
              //fprintf(stderr, "Il file ci sta! :D\n");
              spazioOccupato+=lentmp;
              pthread_mutex_unlock(&mutexQueueFiles);

              char* buftmp = malloc(sizeof(char) * lentmp);

              //newfile->buffer = malloc(sizeof(char) * newfile->length);
              if (readn(connfd, buftmp, lentmp * sizeof(char))<=0) { fprintf(stderr, "sbagliato2\n"); }
              fprintf(stderr, "length file %d\n", lentmp);

              //ora dobbiamo fare la scrittura

              if(newfile->buffer == NULL) { //file vuoto, prima scrittura
                newfile->length = lentmp;
                newfile->buffer = buftmp;
              } else { //file già pieno, appendToFile
                fprintf(stderr, "scrittura in append\n");
                newfile->buffer = realloc(newfile->buffer, sizeof(char) * (lentmp + newfile->length));
                for(int i = 0; i < lentmp; i++)
                  newfile->buffer[i + newfile->length] = buftmp[i];
                newfile->length+=lentmp;
              }
              ris = 0; //tutto bene

              fprintf(stderr, "risposta %d\n", ris);
              printQueueFiles(queueFiles);


              if (writen(connfd, &ris, sizeof(int))<=0) { perror("c"); }
            }
          }
          pthread_mutex_unlock(&newfile->lock);
        }


        break;
      }
      case 'c': {
        pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro);

        int risposta;
        if(esiste == NULL) {
          pthread_mutex_unlock(&mutexQueueFiles);
          fprintf(stderr, "errore, file %s NON rimosso dal server (non esisteva)\n", parametro);
          risposta = -1;
        } else { //file esiste
          fileRAM *tmpfileramtrash = esiste->data;
          if(tmpfileramtrash->is_locked != connfd) { //file esiste ma non lockato
            fprintf(stderr, "File %s non rimosso dal server perchè non aperto dal client\n", parametro);
            risposta = -1;
          } else { //file esiste e lockato, deve essere rimosso
            spazioOccupato-= tmpfileramtrash->length;
            risposta = removeFromQueue(&queueFiles, esiste);
            fprintf(stderr, "file %s rimosso con successo dal server\n", parametro);
          }
          pthread_mutex_unlock(&mutexQueueFiles);

        }
        printQueueFiles(queueFiles);
        if (writen(connfd, &risposta, sizeof(int))<=0) { perror("c"); }
        break;
      }
      case 'r': {
        fprintf(stderr, "Ho ricevuto un comando di lettura!\n");
        pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro);
        int len; //RISPOSTA che ci dice se è tutto ok o no
        if(esiste != NULL) { //file esistente nel server
          fileRAM *filetmp = esiste->data;
          pthread_mutex_lock(&filetmp->lock);
          pthread_mutex_unlock(&mutexQueueFiles);
          if(filetmp->is_locked == connfd) { //file aperto da quel client
            len = filetmp->length;
            char* buf = filetmp->buffer;
            //fprintf(stderr, "sto inserendo %d\n", len);
            if (writen(connfd, &len, sizeof(int))<=0) { perror("c"); }
            if (writen(connfd, buf, len * sizeof(char))<=0) { perror("x"); }
            fprintf(stderr, "file %s letto con successo dal server\n", parametro);
          } else { //file non aperto da quel client
            len = -1;
            if (writen(connfd, &len, sizeof(int))<=0) { perror("c"); }
            fprintf(stderr, "file %s NON letto dal server, non aperto da quel client\n", parametro);
          }
          pthread_mutex_unlock(&filetmp->lock);
        } else { //ERRORE: file non trovato nel server
          pthread_mutex_unlock(&mutexQueueFiles);
          len = -1;
          if (writen(connfd, &len, sizeof(int))<=0) { perror("c"); }
          fprintf(stderr, "errore, file %s NON letto dal server (non esisteva)\n", parametro);
        }
        break;
      }
      case 'R': {
        fprintf(stderr, "Ho ricevuto un comando readNFiles con n = %d\n", atoi(parametro));
        int numDaLeggere;

        if(atoi(parametro) > queueFiles->len)
          numDaLeggere = queueFiles->len;
        else
          numDaLeggere = atoi(parametro);
        if(numDaLeggere <= 0)
          numDaLeggere = queueFiles->len;
        if (writen(connfd, &numDaLeggere, sizeof(int))<=0) { perror("c"); }
        Node* nodetmp = queueFiles->head;
        fileRAM *fileramtmp;
        char* buftmp;
        for(int i = 0; i < numDaLeggere; i++) {
          fileramtmp = nodetmp->data;
          buftmp = malloc(sizeof(char) * strlen(fileramtmp->nome));
          strcpy(buftmp, fileramtmp->nome);
          int buftmplen = strlen(buftmp);
          if (writen(connfd, &buftmplen, sizeof(int))<=0) { perror("c"); }
          if (writen(connfd, buftmp, buftmplen * sizeof(char))<=0) { perror("x"); }
          fprintf(stderr, "sto mandando il file %s al client\n", buftmp);
          free(buftmp);
          nodetmp = nodetmp->next;
        }
        break;
      }
      case 'o': { //openFile
        fprintf(stderr, "ho ricevuto un comando di esistenza di un file %s nel server\n", parametro);
        int risposta;
        pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro);
        /*if(esiste == NULL) //il file non esiste
          risposta = -1;
        else //il file esiste
          risposta = 0;*/
        int flags;
        if (readn(connfd, &flags, sizeof(int))<=0) { fprintf(stderr, "sbagliato2\n"); }
        fprintf(stderr, "codice flags %d\n", flags);
        if(esiste == NULL && flags == 0) //deve aprire il file ma non esiste, errore
          risposta = -1;
        else if(esiste == NULL && flags == 1) { //deve creare e aprire il file (che non esiste)
          if(queueFiles->len + 1 > numeroFile) { //deve iniziare ad espellere file
            fprintf(stderr, "il server è pieno (di numero), cancello un file\n");
            fileRAM *fileramtmptrash = pop(&queueFiles);
            fprintf(stderr, "Sto espellendo il file %s dal server\n", fileramtmptrash->nome);

            //vanno fatte FREE
          }

          fileRAM *newfile = malloc(sizeof(fileRAM));
          pthread_mutex_init(&newfile->lock, NULL);
          newfile->nome = malloc(sizeof(char) * (strlen(basename(parametro)) + 1));
          strcpy(newfile->nome, basename(parametro));
          newfile->nome[strlen(basename(parametro))] = '\0';
          newfile->length = 0;
          newfile->buffer = NULL;
          newfile->is_locked = connfd;
          push(&queueFiles, newfile);
          risposta = 0;
        }
        else if(esiste != NULL && flags == 0) { //deve aprire il file, che esiste già
          fileRAM *fileramtmp = esiste->data;
          if(fileramtmp->is_locked != -1)
            risposta = -1;
          else {
            fileramtmp->is_locked = connfd;
            risposta = 0;
          }
        } else if(esiste != NULL && flags == 1) { //deve creare e aprire il file, che esiste già, errore
          risposta = -1;
        }
        pthread_mutex_unlock(&mutexQueueFiles);
        if (writen(connfd, &risposta, sizeof(int))<=0) { perror("c"); }
        fprintf(stderr, "ho finito la open di %s\n", parametro);
        break;
      }
      case 'z': { //closeFile
        int ris;
        pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro);
        if(esiste == NULL) { //file da chiudere non esiste nel server
          pthread_mutex_unlock(&mutexQueueFiles);
          ris = -1;
        } else { //file da chiudere esiste nel server
          fileRAM *fileramtmp = esiste->data;
          pthread_mutex_lock(&fileramtmp->lock);
          if(fileramtmp->is_locked != connfd) { //errore: file non aperto da quel client
            ris = -1;
          } else { //chiudo il file
            fileramtmp->is_locked = -1;
            ris = 0; //tutto ok
          }
          pthread_mutex_unlock(&fileramtmp->lock);
          pthread_mutex_unlock(&mutexQueueFiles);
        }
        if (writen(connfd, &ris, sizeof(int))<=0) { perror("c"); }
        break;
      }
    }

    //free(risposta);

    /*char* prova = "messaggio di prova";
    int lenprova = strlen(prova);
    if (writen(connfd, &lenprova, sizeof(int))<=0) { perror("c"); }
    //str.str="messaggio";
    if (writen(connfd, prova, strlen(prova)*sizeof(char))<=0) { perror("x"); }*/

    //IMPORTANTE: ALLA FINE DELLA RICHIESTA RIAGGIUNGERE ALL'FD_SET
    FD_SET(connfd, &set);
    //FD_SET(p[*numthread][1], &set); //devo capire dove rimettere questa riga di codice, perchè così non va
    fprintf(stderr, "num thread %d, connfd %ld\n", numthread, connfd);
    write(p[numthread][1], "finito", 6);

    fprintf(stderr, "ho finito la scrittura, connfd %ld\n", connfd);


    /*fprintf(stderr, "comando %c resto del file passato %s\n", comando, str.str);
    //toup(str.str);
    if (writen(connfd, &str.len, sizeof(int))<=0) { free(str.str); }
    //str.str="messaggio";
    if (writen(connfd, str.str, str.len*sizeof(char))<=0) { free(str.str); }
    free(str.str);*/

  }
  return NULL;
}

void printQueueClient(Queue *q) {
  Node* tmp = q->head;
  ComandoClient *no = NULL;
  while(tmp != NULL) {
    no = tmp->data;
    fprintf(stdout, "parametro %s comando %c connfd %ld\n", no->parametro, no->comando, no->connfd);
    tmp = tmp->next;
  }
}

static void* tSegnali(void* arg) {
  //control + C = segnale 2, SIGINT
  //control + \ = segnale 3, SIGQUIT
  //SIGINT uguale a SIGQUIT
  //kill -1 pid = segnale 1, SIGHUP
  sigset_t *mask = (sigset_t*)arg;
  fprintf(stderr, "sono il thread gestore segnali\n");
  int segnalericevuto;
  sigwait(mask, &segnalericevuto);
  fprintf(stderr, "segnale ricevuto %d\n", segnalericevuto);
  if(segnalericevuto == 2 || segnalericevuto == 3) { //gestione SIGINT
    rsigint = 1;

  } else if(segnalericevuto == 1) { //gestione SIGHUP
    rsighup = 1;
  }
  write(psegnali[1], "segnale", 7);
  fprintf(stderr, "scritto nella pipe il segnale\n");
  return NULL;
}

int main(int argc, char* argv[]) {
  parser();


  //GESTIONE SEGNALI
  rsigint = 0; //inizialmente non ho ricevuto un segnale SIGINT
  rsighup = 0; //inizialmente non ho ricevuto un segnale SIGHUP
  struct sigaction sahup;
  struct sigaction saquit;
  struct sigaction saint; //control+C
  memset(&sahup, 0, sizeof(sigaction));
  memset(&saquit, 0, sizeof(sigaction));
  memset(&saint, 0, sizeof(sigaction));
  /*sahup.sa_handler = sighandlerhup;
  saquit.sa_handler = sighandlerquit;
  saint.sa_handler = sighandlerquit;*/
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGHUP);
  sigaddset (&mask, SIGQUIT);
  sigaddset (&mask, SIGINT);
  pthread_sigmask(SIG_SETMASK, &mask, NULL);
  pthread_t tGestoreSegnali;
  pthread_create(&tGestoreSegnali, NULL, tSegnali, (void*)&mask);
  psegnali = (int*)malloc(sizeof(int) * 2);
  if(pipe(psegnali) == -1) { perror("pipe"); }
  //close(psegnali[1]);

  numWorkers = 3;
  cleanup();
  atexit(cleanup);
  queueClient = initQueue(); //coda dei file descriptor dei client che provano a connettersi
  queueFiles = initQueue();


  pthread_t *t = malloc(sizeof(pthread_t) * numWorkers); //array dei thread
  p = (int**)malloc(sizeof(int*) * numWorkers); //array delle pipe
  int *arrtmp = malloc(sizeof(int) * numWorkers);
  for(int i = 0; i < numWorkers; i++) {
    //int *numthread = malloc(sizeof(int));
    //*numthread = &i;
    arrtmp[i] = i; //per passarlo al thread
    //int numthread = i;
    //fprintf(stderr, "sto passando i %d, numthread %d\n", i, numthread);
    pthread_create(&t[i], NULL, threadF, (void*)&(arrtmp[i]));
    //dichiaro numWorkers pipe

    //int *pfd = malloc(sizeof(int) * 2);


    //int pfd[2];
    //if(pipe(pfd) == -1) { perror("pipe"); }

    p[i] = (int*)malloc(sizeof(int) * 2);

    if(pipe(p[i]) == -1) { perror("pipe"); }
    //close((p[i])[1]);
    //p[i] = pfd;
    fprintf(stderr, "indirizzo pipe lettura %d scrittura %d\n", p[i][0], p[i][1]);
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

  fd_set tmpset; //fd che voglio aspettare
  // azzero sia il master set che il set temporaneo usato per la select
  FD_ZERO(&set);
  FD_ZERO(&tmpset);

  // aggiungo il listener fd al master set
  FD_SET(listenfd, &set); //voglio ricevere informazioni da listenfd (socket principale)
  int fdmax = listenfd;

  for(int i = 0; i < numWorkers; i++) {
    FD_SET(p[i][0], &set); //p array di pipe, aggiungo tutte le pipe alla set in lettura
    if(p[i][0] > fdmax)
      fdmax = p[i][0];
    fprintf(stderr, "inserisco il connfd della pipe %d\n", p[i][0]);
  }

  //inserisco la pipe dei segnali nella set
  FD_SET(psegnali[0], &set);
  if(psegnali[0] > fdmax)
    fdmax = psegnali[0];

  // tengo traccia del file descriptor con id piu' grande
  //fprintf(stderr, "ciao\n");
  while(!rsigint) { //SIGINT CONTROL, alla chiusura del for aspetto che i thread terminino
// copio il set nella variabile temporanea per la select
    //fprintf(stderr, "sono nel for in alto\n");
    tmpset = set;
    if (select(fdmax+1, &tmpset, NULL, NULL, NULL) == -1) { //quando esco dalla select, sono sicuro che almeno uno dei file nella set ha qualcosa da leggere, sia accettare una nuova connessione che nuova richiesta dai client
      //in tmpset a questo punto abbiamo solo le cose effettivamente modificate
      perror("select");
      return -1;
    }

// cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
//fprintf(stderr, "fdmax %d\n", fdmax);
    for(int i=0; i <= fdmax; i++) {
      //fprintf(stderr, "ciao7\n");
      if (FD_ISSET(i, &tmpset)) { //ora so che qualcosa è pronto, ma non so cosa, e devo capire se l'i-esimo è pronto
        long connfd;
        if (i == listenfd) { //SIGHUP CONTROL // e' una nuova richiesta di connessione
          //SYSCALL_EXIT("accept", connfd, accept(listenfd, (struct sockaddr*)NULL ,NULL), "accept", "");
          connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
          FD_SET(connfd, &set);  // aggiungo il descrittore al master set
          if(connfd > fdmax)
            fdmax = connfd;  // ricalcolo il massimo
          continue;
        }
        //ELSE
        connfd = i;  // e' una nuova richiesta da un client già connesso o da una pipe

        //gestione segnali
        if(connfd == psegnali[0]) { //se è vero, ho ricevuto un segnale
          fprintf(stderr, "ricevuto segnale nel main\n");
          char buftmp[8];
          read(connfd, buftmp, 7);
          pthread_cond_broadcast(&condQueueClient);
          //va gestito
          /*if(rsigint) { //il segnale ricevuto è SIGINT

          }*/
          continue;
        }

        //controllo se è una delle pipe
        int ispipe = 0;
        for(int j = 0; j < numWorkers; j++) {
          //fprintf(stderr, "sono nel for, connfd %ld p[j][0] %d\n", connfd, p[j][0]);
          if(connfd == p[j][0]) {
            //fprintf(stderr, "è una pipe\n");
            char buftmp[7];
            read(connfd, buftmp, 6);
            fprintf(stderr, "è una pipe\n");
            //fprintf(stderr, "è una pipe, ho letto dalla read %s, connfd %ld\n", buftmp, connfd);
            ispipe = 1;
            //FD_CLR(connfd, &set);
            //FD_SET(connfd, &set);  // aggiungo il descrittore al master set
          }
        }
        if(ispipe)
          continue;

        //READ DI CONNFD
        //OTTENGO IL TIPO DI OPERAZIONE DAL CLIENT CONNFD
        //PUSH(CODAOPERAZIONI, OPERAZIONE_CONNFD)
        //FD_CLR(connfd, &set) //tolgo dalla set il fd del client finchè non ho gestito l'intera sua richiesta
        //MANDO IL SEGNALE
        //THREAD ESEGUE TUTTO IL COMANDO DEL CLIENT E POI RIMETTE CONNFD NEL SET


  // eseguo il comando e se c'e' un errore lo tolgo dal master set
        //if (cmd(connfd) < 0) {
        /*if (-1 < 0) {
          close(connfd);
          FD_CLR(connfd, &set);
    // controllo se deve aggiornare il massimo
          if (connfd == fdmax)
            fdmax = updatemax(set, fdmax);
        }*/
        fprintf(stderr, "ho ricevuto una nuova richiesta da %ld\n", connfd);

        FD_CLR(connfd, &set);

        msg_t str;
        char comando;
        int r = readn(connfd, &str.len, sizeof(int));
        if(r == 0) { //il socket è vuoto

          fprintf(stderr, "client disconnesso\n");
          if (connfd == fdmax)
            fdmax = updatemax(set, fdmax);
          if(areThereConnections(tmpset, fdmax))
            fprintf(stderr, "ci sono connessioni attive\n");
          else
            fprintf(stderr, "non ci sono altre connessioni\n");
          //fprintf(stderr, "risultato connessione 3: %d\n", FD_ISSET(3, &set));
          //fd_set setsignal;
          //FD_ZERO(&setsignal);
          //setsignal = tmpset;
          //if(disconnecting == 1) then for...
          /*for(int z = 0; z < fdmax; z++) {
            if (FD_ISSET(z, &tmpset)) {
              fprintf(stderr, "%d connesso\n", z);
            } else fprintf(stderr, "%d NON connesso\n", z);
            fprintf(stderr, "risultato connessione %d: %d\n", z, FD_ISSET(z, &setsignal));
          }*/

          continue;
        }
        if (r<0) { fprintf(stderr, "sbagliato1\n"); }
        str.len = str.len - sizeof(char);
        //togliamo sizeof(char) perchè nella read al comando prima stiamo leggendo già un carattere
        if (readn(connfd, &comando, sizeof(char))<=0) { fprintf(stderr, "sbagliato4\n"); }

        //fprintf(stderr, "fin qui, ho letto la lunghezza %d\n", str.len);
        str.str = calloc((str.len), sizeof(char));
        if (!str.str) {
    	     perror("calloc");
    	      fprintf(stderr, "Memoria esaurita....\n");
    	       //return NULL;
        }
        if (readn(connfd, str.str, (str.len)*sizeof(char))<=0) { fprintf(stderr, "sbagliato2\n"); }
        //if (readn(connfd, str.str, (str.len - sizeof(char))*sizeof(char))<=0) { fprintf(stderr, "sbagliato2\n"); }
        //togliamo sizeof(char) perchè nella read al comando prima stiamo leggendo già un carattere
        //fprintf(stderr, "fin qui2222\n");

        //inserisco in coda il comando letto
        ComandoClient *cmdtmp = malloc(sizeof(ComandoClient));
        cmdtmp->comando = comando;
        cmdtmp->parametro = malloc(sizeof(char) * (strlen(str.str)) + 1);
        cmdtmp->connfd = connfd;
        strcpy(cmdtmp->parametro, str.str);
        cmdtmp->parametro[strlen(str.str)] = '\0';
        fprintf(stderr, "il parametro è %s\n", cmdtmp->parametro);

        pthread_mutex_lock(&mutexQueueClient);
        fprintf(stderr, "parte1\n");
        //printQueueClient(queueClient);
        fprintf(stderr, "parte32482\n");

        push(&queueClient, cmdtmp);
        fprintf(stderr, "parte2\n");

        pthread_mutex_unlock(&mutexQueueClient);
        pthread_cond_signal(&condQueueClient);


        /*fprintf(stderr, "comando %c resto del file passato %s\n", comando, str.str);
        //toup(str.str);
        if (writen(connfd, &str.len, sizeof(int))<=0) { free(str.str); }
        //str.str="messaggio";
        if (writen(connfd, str.str, str.len*sizeof(char))<=0) { free(str.str); }
        free(str.str);*/





        //fprintf(stderr, "connfd originale %ld\n", connfd);
        //push(&queueClient, connfd);
        //pthread_cond_signal(&condQueueClient);
        //fprintf(stderr, "ciao2\n");
        //printf("inserito\n");
      }
    }
  }
  fprintf(stderr, "sono fuori dal for\n");
  //chiusura socket, pipe
}
