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
#include "util.h"

//"nomeInfo valore"
//"spazio 1000"
//"numeroFIle 100"
#define MAXBUFFER 1000
#define MAXSTRING 100
#define CONFIGFILE "./configs/config.txt"
#define SPAZIO "spazio"
#define NUMEROFILE "numeroFile"
#define SOC "sockName"
#define WORK "numWorkers"
#define MAXBACKLOG 32

int spazio = 0;
int numeroFile = 0;
int numWorkers = 0;
char* SockName = NULL;
Queue *queueClient; //coda dei comandi gestiti dai thread
Queue *queueFiles; //coda dei file memorizzati

int spazioOccupato = 0;

//statistiche
typedef struct _stats {
  int numMaxMemorizzato;
  double spazioMaxOccupato;
  int numAlgoRimpiazzamento;
  pthread_mutex_t mutexStats;
} Statistiche;

Statistiche *s;

/*int numMaxMemorizzato;
int spazioMaxOccupato;
int numAlgoRimpiazzamento;
static pthread_mutex_t mutexStats = PTHREAD_MUTEX_INITIALIZER;*/

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


/*typedef struct msg {
    int len;
    char *str;
} msg_t;*/

void cleanup() {
  if(SockName != NULL) {
    unlink(SockName);
    free(SockName);
  }
}

int updatemax(fd_set set, int fdmax) {
  //fprintf(stderr, "sono nell'updatemax\n");
  for(int i = (fdmax - 1);i >= 0; --i)
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



void parser(char* configfile) {
  char* a = NULL;
  int i;
  char* save;
  char* token;
  //int spazio = 0;
  //int numeroFile = 0;
  //int numWorkers = 0;
  //char* SockName;
  char* buffer;
  ec_null((buffer = malloc(sizeof(char) * MAXBUFFER)), "malloc");
  FILE* p;
  //fprintf(stderr,"NON abbiamo aperto il file\n");
  ec_null((p = fopen(configfile, "r")), "fopen");

  //fprintf(stderr, "abbiamo aperto il file\n");
  while(fgets(buffer, MAXBUFFER, p)) {
    //fprintf(stderr, "ecco il buffer: %s\n", buffer);
        //ora facciamo il parser della singola riga

            //tokenizzo la stringa e vedo il numero di virgole
    save = NULL;
    //fprintf(stderr, "ultimo carattere %d, buffer %s\n", buffer[strlen(buffer) - 1], buffer);
    buffer[strlen(buffer) - 1] = '\0';
    token = strtok_r(buffer, " ",  &save);
    char** tmp;
    ec_null((tmp = malloc(sizeof(char*) * 2)), "malloc");
    i = 0;
    while(token) {
      //fprintf(stderr, "sto processando %s\n", token);
      ec_null((tmp[i] = malloc(sizeof(char) * MAXSTRING)), "malloc");
      strcpy(tmp[i], token); //se è il secondo elemento (i = i) non deve prendere il newline finale,
      //fprintf(stderr, "tmp : %s\n", tmp[i]);
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

      ec_null((SockName = malloc(sizeof(char) * (strlen(tmp[1]) + 1))), "malloc");
      strcpy(SockName, tmp[1]);
      //fprintf(stderr, "strlen %d\n",strlen(tmp[1]));
      SockName[strlen(tmp[1])] = '\0';
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
  if(fclose(p) != 0) { perror("fclose"); exit(EXIT_FAILURE); }



  free(buffer);

  if(spazio <= 0 || numeroFile <= 0 || SockName == NULL || numWorkers <= 0) {
    fprintf(stderr, "config.txt errato\n");
    exit(EXIT_FAILURE);
  }
  //fprintf(stderr,"abbiamo chiuso il file\n");
  //if(verbose) {
  fprintf(stdout,"Spazio: %d\n", spazio);
  fprintf(stdout,"numeroFile: %d\n", numeroFile);
  fprintf(stdout,"SockName: %s\n", SockName);
  fprintf(stdout,"numWorkers: %d\n", numWorkers);
  //}
}

void printQueueFiles(Queue *q) {
  Node* tmp = q->head;
  fileRAM *no = NULL;
  fprintf(stdout, "Coda dei file:\n");
  while(tmp != NULL) {
    no = tmp->data;
    fprintf(stdout, "filename %s, length %ld\n", no->nome, no->length);
    tmp = tmp->next;
  }
  fprintf(stdout, "Fine stampa coda dei file\n");
}

Node* fileExistsInServer(Queue *q, char* nomefile) {
  //pthread_mutex_lock(&mutexQueueFiles);
  if(q == NULL || nomefile == NULL) {
    errno = EINVAL;
    perror("fileExistsInServer");
    exit(EXIT_FAILURE);
  }
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
  fprintf(stdout, "Sono il thread %d\n", numthread);
  //fprintf(stderr, "ciao\n");
  while(1) {
    Pthread_mutex_lock(&mutexQueueClient);
    while(queueClient->len == 0 && !rsigint) {
      if(pthread_cond_wait(&condQueueClient, &mutexQueueClient) != 0) { perror("pthread_cond_wait"); exit(EXIT_FAILURE); }
      fprintf(stdout, "Sono sveglio (num thread %d)!\n", numthread);
      }
    //gestione segnali
    //fprintf(stderr, "ricevuto segnale\n");
    if(rsigint == 1) {
      Pthread_mutex_unlock(&mutexQueueClient);
      fprintf(stderr, "Sono il thread %d e ho ricevuto un segnale\n", numthread);
      break;
    }
    //if(rsighup == 1 && q->len == 0)

    ComandoClient* tmp = pop(&queueClient);
    //fprintf(stderr, "lunghezza lista %lu\n", queueClient->len);
    Pthread_mutex_unlock(&mutexQueueClient);


    //fprintf(stderr, "ciaoyguyj\n");
    long connfd = tmp->connfd;
    char comando = tmp->comando;
    char* parametro = tmp->parametro;

    fprintf(stdout, "Sto eseguendo il comando %c che ha parametro %s del connfd %ld\n", comando, parametro, connfd);
    //print
    //char* risposta = malloc(sizeof(char) * MAXBUFFER);
    //int lenRisposta;
    int notused;

    //POSSIBILI COMANDI PASSATI
    switch(comando) {
      case 'W': { //richiesta di scrittura
        fprintf(stdout, "Ho ricevuto un comando di scrittura del file %s\n", parametro);
        //fprintf(stderr, "sono nello writch\n");
        Pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro);
        Pthread_mutex_unlock(&mutexQueueFiles);

        int ris = 0;
        if(esiste == NULL) { //errore: il file non esiste (dovrebbe già essere stato creato dalla openFile)
          ris = -1;
          //if (writen(connfd, &ris, sizeof(int))<=0) { perror("c"); }
          SYSCALL_EXIT("writen", notused, writen(connfd, &ris, sizeof(int)), "write", "");
        } else { //file esiste
          fileRAM *newfile = esiste->data;
          Pthread_mutex_lock(&newfile->lock);
          if(newfile->is_locked != connfd) { //file eistse ma non aperto dal client
            ris = -1;
          }
          //if (writen(connfd, &ris, sizeof(int))<=0) { perror("c"); }
          SYSCALL_EXIT("writen", notused, writen(connfd, &ris, sizeof(int)), "write", "");

          if(ris != -1) {
            int lentmp;
            SYSCALL_EXIT("readn", notused, readn(connfd, &lentmp, sizeof(int)), "readn", "");
            //notused = readn(connfd, &lentmp, sizeof(int));
            int cista = 1; //ci sta nel server fino a prova contraria
            if(lentmp > spazio || lentmp + newfile->length > spazio) { //non lo memorizza proprio
              //il secondo controllo dopo l'or è per vedere se l'append di un file non ci sta nel server
              fprintf(stderr, "Il file %s è troppo grande (%ld) e non sta materialmente nel server (capienza massima %d)\n", newfile->nome, newfile->length, spazio);

                //vado a scrivere nel socket che il file non ci sta
              if(lentmp > spazio) //se il file non esiste ancora (ha size null)
                removeFromQueue(&queueFiles, esiste);

              free(newfile->nome);
              free(newfile);
              //free(esiste); no, la fa già removefromqueue
              cista = 0;
            }
            //if (writen(connfd, &cista, sizeof(int))<=0) { perror("c"); }
            SYSCALL_EXIT("writen", notused, writen(connfd, &cista, sizeof(int)), "write", "");

            if(cista) {
              //fprintf(stderr, "UEEEEEEE\n");
              fileRAM *fileramtmptrash;
              Pthread_mutex_lock(&mutexQueueFiles);

              //aggiorno le statistiche
              Pthread_mutex_lock(&s->mutexStats);
              /*s->numMaxMemorizzato = 0;
              s->spazioMaxOccupato = 0;
              s->numAlgoRimpiazzamento = 0;*/
              if(spazioOccupato + lentmp > spazio)
                s->numAlgoRimpiazzamento++;
              Pthread_mutex_unlock(&s->mutexStats);

              while(spazioOccupato + lentmp > spazio) { //deve iniziare ad espellere file
                fprintf(stderr, "Il server è pieno (di spazio). Devo espellere file.\n");
                fileRAM *firstel = returnFirstEl(queueFiles);
                if(newfile != firstel) { //non rimuove il primo elemento perchè è proprio quello che dobbiamo inserire
                  fileramtmptrash = pop(&queueFiles);
                } else { //rimuove il secondo elemento
                  fileramtmptrash = pop2(&queueFiles);
                }
                fprintf(stderr, "Sto espellendo il file %s dal server per far spazio al file %s\n", fileramtmptrash->nome, parametro);
                spazioOccupato-=fileramtmptrash->length;
                free(fileramtmptrash->nome);
                if(fileramtmptrash->buffer != NULL) //il buffer potrebbe non esistere
                  free(fileramtmptrash->buffer);
                free(fileramtmptrash);
                  //vanno fatte FREE
              }
              //fprintf(stderr, "Il file ci sta! :D\n");
              spazioOccupato+=lentmp;
              Pthread_mutex_unlock(&mutexQueueFiles);

              char* buftmp;
              ec_null((buftmp = malloc(sizeof(char) * lentmp)), "malloc");

              //newfile->buffer = malloc(sizeof(char) * newfile->length);
              SYSCALL_EXIT("readn", notused, readn(connfd, buftmp, lentmp * sizeof(char)), "read", "");
              //if (readn(connfd, buftmp, lentmp * sizeof(char))<=0) { fprintf(stderr, "sbagliato2\n"); }
              //fprintf(stderr, "length file %d\n", lentmp);

              //ora dobbiamo fare la scrittura

              if(newfile->buffer == NULL) { //file vuoto, prima scrittura
                newfile->length = lentmp;
                newfile->buffer = buftmp;
              } else { //file già pieno, appendToFile
                fprintf(stderr, "Scrittura in append\n");
                //newfile->buffer = realloc(newfile->buffer, sizeof(char) * (lentmp + newfile->length));
                ec_null((newfile->buffer = realloc(newfile->buffer, sizeof(char) * (lentmp + newfile->length))), "realloc");
                for(int i = 0; i < lentmp; i++)
                  newfile->buffer[i + newfile->length] = buftmp[i];
                newfile->length+=lentmp;
                free(buftmp);
              }
              ris = 0; //tutto bene

              //aggiorno le statistiche
              Pthread_mutex_lock(&s->mutexStats);
              if(queueFiles->len > s->numMaxMemorizzato)
                s->numMaxMemorizzato = queueFiles->len;
              if(spazioOccupato > s->spazioMaxOccupato)
                s->spazioMaxOccupato = spazioOccupato;
              Pthread_mutex_unlock(&s->mutexStats);


              //fprintf(stderr, "risposta %d\n", ris);
              printQueueFiles(queueFiles);

              SYSCALL_EXIT("writen", notused, writen(connfd, &ris, sizeof(int)), "write", "");
              //if (writen(connfd, &ris, sizeof(int))<=0) { perror("c"); }
            }
          }
          Pthread_mutex_unlock(&newfile->lock);
        }


        break;
      }
      case 'c': {
        fprintf(stdout, "Ho ricevuto un comando di cancellazione del file %s\n", parametro);
        Pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro);

        int risposta;
        if(esiste == NULL) {
          Pthread_mutex_unlock(&mutexQueueFiles);
          fprintf(stderr, "Errore: file %s NON rimosso dal server (non esisteva)\n", parametro);
          risposta = -1;
        } else { //file esiste
          fileRAM *tmpfileramtrash = esiste->data;
          if(tmpfileramtrash->is_locked != connfd) { //file esiste ma non lockato
            fprintf(stderr, "Errore: file %s non rimosso dal server perchè non aperto dal client\n", parametro);
            risposta = -1;
          } else { //file esiste e lockato, deve essere rimosso
            spazioOccupato-= tmpfileramtrash->length;
            risposta = removeFromQueue(&queueFiles, esiste);
            free(tmpfileramtrash->nome);
            if(tmpfileramtrash->buffer != NULL)
              free(tmpfileramtrash->buffer);
            free(tmpfileramtrash);
            fprintf(stdout, "File %s rimosso con successo dal server\n", parametro);
          }
          Pthread_mutex_unlock(&mutexQueueFiles);

        }
        printQueueFiles(queueFiles);
        SYSCALL_EXIT("writen", notused, writen(connfd, &risposta, sizeof(int)), "write", "");
        //if (writen(connfd, &risposta, sizeof(int))<=0) { perror("c"); }
        break;
      }
      case 'r': {
        fprintf(stdout, "Ho ricevuto un comando di lettura del file %s\n", parametro);
        Pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro);
        int len; //RISPOSTA che ci dice se è tutto ok o no
        if(esiste != NULL) { //file esistente nel server
          fileRAM *filetmp = esiste->data;
          Pthread_mutex_lock(&filetmp->lock);
          Pthread_mutex_unlock(&mutexQueueFiles);
          if(filetmp->is_locked == connfd) { //file aperto da quel client
            len = filetmp->length;
            char* buf = filetmp->buffer;
            //fprintf(stderr, "sto inserendo %d\n", len);
            SYSCALL_EXIT("writen", notused, writen(connfd, &len, sizeof(int)), "write", "");
            SYSCALL_EXIT("writen", notused, writen(connfd, buf, len * sizeof(char)), "write", "");
            //if (writen(connfd, &len, sizeof(int))<=0) { perror("c"); }
            //if (writen(connfd, buf, len * sizeof(char))<=0) { perror("x"); }
            fprintf(stdout, "File %s letto con successo dal server\n", parametro);
          } else { //file non aperto da quel client
            len = -1;
            SYSCALL_EXIT("writen", notused, writen(connfd, &len, sizeof(int)), "write", "");
            //if (writen(connfd, &len, sizeof(int))<=0) { perror("c"); }
            fprintf(stderr, "Errore: file %s NON letto dal server perchè non aperto dal client\n", parametro);
          }
          Pthread_mutex_unlock(&filetmp->lock);
        } else { //ERRORE: file non trovato nel server
          Pthread_mutex_unlock(&mutexQueueFiles);
          len = -1;
          SYSCALL_EXIT("writen", notused, writen(connfd, &len, sizeof(int)), "write", "");
          //if (writen(connfd, &len, sizeof(int))<=0) { perror("c"); }
          fprintf(stderr, "Errore: file %s NON letto dal server (non esisteva)\n", parametro);
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
        SYSCALL_EXIT("writen", notused, writen(connfd, &numDaLeggere, sizeof(int)), "write", "");
        //if (writen(connfd, &numDaLeggere, sizeof(int))<=0) { perror("c"); }
        Node* nodetmp = queueFiles->head;
        fileRAM *fileramtmp;
        char* buftmp;
        for(int i = 0; i < numDaLeggere; i++) {
          fileramtmp = nodetmp->data;
          ec_null((buftmp = malloc(sizeof(char) * strlen(fileramtmp->nome))), "malloc");
          strcpy(buftmp, fileramtmp->nome);
          int buftmplen = strlen(buftmp);
          SYSCALL_EXIT("writen", notused, writen(connfd, &buftmplen, sizeof(int)), "write", "");
          SYSCALL_EXIT("writen", notused, writen(connfd, buftmp, buftmplen * sizeof(char)), "write", "");
          //if (writen(connfd, &buftmplen, sizeof(int))<=0) { perror("c"); }
          //if (writen(connfd, buftmp, buftmplen * sizeof(char))<=0) { perror("x"); }
          //fprintf(stdout, "Ho mandato il file %s al client\n", buftmp);
          free(buftmp);
          nodetmp = nodetmp->next;
        }
        break;
      }
      case 'o': { //openFile
        fprintf(stderr, "Ho ricevuto un comando di esistenza del file %s nel server\n", parametro);
        int risposta;
        Pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro);
        /*if(esiste == NULL) //il file non esiste
          risposta = -1;
        else //il file esiste
          risposta = 0;*/
        int flags;
        SYSCALL_EXIT("readn", notused, readn(connfd, &flags, sizeof(int)), "read", ""); //passo i flag che deve usare la open
        //if (readn(connfd, &flags, sizeof(int))<=0) { fprintf(stderr, "sbagliato2\n"); }
        //fprintf(stderr, "codice flags %d\n", flags);
        if(esiste == NULL && flags == 0) { //deve aprire il file ma non esiste, errore
          risposta = -1;
          //fprintf(stderr, "Errore: il file %s non esiste!\n", parametro);
        } else if(esiste == NULL && flags == 1) { //deve creare e aprire il file (che non esiste)
          if(queueFiles->len + 1 > numeroFile) { //deve iniziare ad espellere file
            fprintf(stdout, "Il server è pieno (di numero file), ne cancello uno\n");
            fileRAM *fileramtmptrash = pop(&queueFiles);
            fprintf(stdout, "Sto espellendo il file %s dal server\n", fileramtmptrash->nome);

            //vanno fatte FREE
          }

          fileRAM *newfile;
          ec_null((newfile = malloc(sizeof(fileRAM))), "malloc");
          //fileRAM *newfile = malloc(sizeof(fileRAM));
          if(pthread_mutex_init(&newfile->lock, NULL) != 0) { perror("pthread_mutex_init"); exit(EXIT_FAILURE); }
          ec_null((newfile->nome = malloc(sizeof(char) * (strlen(basename(parametro)) + 1))), "malloc");
          //newfile->nome = malloc(sizeof(char) * (strlen(basename(parametro)) + 1));
          strcpy(newfile->nome, basename(parametro));
          newfile->nome[strlen(basename(parametro))] = '\0';
          newfile->length = 0;
          newfile->buffer = NULL;
          newfile->is_locked = connfd;
          ec_meno1((push(&queueFiles, newfile)), "push");
          //push(&queueFiles, newfile);
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
          risposta = -2;
        }
        Pthread_mutex_unlock(&mutexQueueFiles);
        //mando al client la risposta della openFile
        SYSCALL_EXIT("writen", notused, writen(connfd, &risposta, sizeof(int)), "write", "");
        //if (writen(connfd, &risposta, sizeof(int))<=0) { perror("c"); }
        if(risposta == 0)
          fprintf(stdout, "Ho aperto con successo il file %s\n", parametro);
        else
          fprintf(stderr, "Errore: non ho aperto il file %s, codice errore %d\n", parametro, risposta);
        break;
      }
      case 'z': { //closeFile
        int ris;
        Pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro);
        if(esiste == NULL) { //file da chiudere non esiste nel server
          Pthread_mutex_unlock(&mutexQueueFiles);
          ris = -1;
        } else { //file da chiudere esiste nel server
          fileRAM *fileramtmp = esiste->data;
          Pthread_mutex_lock(&fileramtmp->lock);
          if(fileramtmp->is_locked != connfd) { //errore: file non aperto da quel client
            ris = -1;
          } else { //chiudo il file
            fileramtmp->is_locked = -1;
            ris = 0; //tutto ok
          }
          Pthread_mutex_unlock(&fileramtmp->lock);
          Pthread_mutex_unlock(&mutexQueueFiles);
        }
        //mando al client la risposta della closeFile
        SYSCALL_EXIT("writen", notused, writen(connfd, &ris, sizeof(int)), "write", "");
        //if (writen(connfd, &ris, sizeof(int))<=0) { perror("c"); }
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
    //fprintf(stderr, "num thread %d, connfd %ld\n", numthread, connfd);
    //write(p[numthread][1], "finito", 6);
    SYSCALL_EXIT("writen", notused, writen(p[numthread][1], "finito", 6), "write", "");

    //fprintf(stderr, "ho finito la scrittura, connfd %ld\n", connfd);


    /*fprintf(stderr, "comando %c resto del file passato %s\n", comando, str.str);
    //toup(str.str);
    if (writen(connfd, &str.len, sizeof(int))<=0) { free(str.str); }
    //str.str="messaggio";
    if (writen(connfd, str.str, str.len*sizeof(char))<=0) { free(str.str); }
    free(str.str);*/
    free(tmp->parametro);
    free(tmp);
  }
  fprintf(stderr, "Sto uscendo dal thread %d\n", numthread);
  //cleanup
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
  if(arg == NULL) {
    errno = EINVAL;
    perror("tSegnali");
    exit(EXIT_FAILURE);
  }
  //control + C = segnale 2, SIGINT
  //control + \ = segnale 3, SIGQUIT
  //SIGINT uguale a SIGQUIT
  //kill -1 pid = segnale 1, SIGHUP

  sigset_t *mask = (sigset_t*)arg;
  fprintf(stdout, "Sono il thread gestore segnali\n");
  int segnalericevuto;
  sigwait(mask, &segnalericevuto);
  fprintf(stdout, "Sono il thread gestore segnali, ho ricevuto il segnale %d\n", segnalericevuto);
  if(segnalericevuto == SIGINT || segnalericevuto == SIGQUIT) { //gestione SIGINT o SIGQUIT, che si controllano allo stesso modo
    rsigint = 1;

  } else if(segnalericevuto == SIGHUP) { //gestione SIGHUP
    rsighup = 1;
  } else {
    tSegnali(arg);
    return NULL;
  }
  int notused;
  SYSCALL_EXIT("writen", notused, writen(psegnali[1], "segnale", 7), "write", "");
  //write(psegnali[1], "segnale", 7);
  fprintf(stdout, "Sono il thread gestore segnali e ho concluso ciò che dovevo fare\n");
  return NULL;
}

int main(int argc, char* argv[]) {
  if(argc != 2) { fprintf(stderr, "ERRORE: passare il file config.txt\n"); exit(EXIT_FAILURE); }
  parser(argv[1]);

  //inizializzo statistiche
  ec_null((s = malloc(sizeof(Statistiche))), "malloc");
  if(pthread_mutex_init(&s->mutexStats, NULL) != 0) { perror("pthread_mutex_init"); exit(EXIT_FAILURE); }
  Pthread_mutex_lock(&s->mutexStats);
  s->numMaxMemorizzato = 0;
  s->spazioMaxOccupato = 0;
  s->numAlgoRimpiazzamento = 0;
  Pthread_mutex_unlock(&s->mutexStats);



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
  sigaddset(&mask, SIGQUIT);
  sigaddset(&mask, SIGINT);
  if(pthread_sigmask(SIG_SETMASK, &mask, NULL) != 0) { perror("pthread_sigmask"); exit(EXIT_FAILURE); }
  pthread_t tGestoreSegnali;
  if(pthread_create(&tGestoreSegnali, NULL, tSegnali, (void*)&mask) != 0) { perror("pthread_sigmask"); exit(EXIT_FAILURE); }
  //psegnali = (int*)malloc(sizeof(int) * 2);
  ec_null((psegnali = (int*)malloc(sizeof(int) * 2)), "malloc");
  ec_meno1((pipe(psegnali)), "pipe");

  //if(pipe(psegnali) == -1) { perror("pipe"); }
  //close(psegnali[1]);

  //numWorkers = 6;
  //ec_meno1(cleanup(), "cleanup");

  //cleanup();
  //atexit(cleanup);
  queueClient = initQueue(); //coda dei file descriptor dei client che provano a connettersi
  queueFiles = initQueue();

  pthread_t *t;
  ec_null((t = malloc(sizeof(pthread_t) * numWorkers)), "malloc");
  //pthread_t *t; = malloc(sizeof(pthread_t) * numWorkers); //array dei thread
  ec_null((p = (int**)malloc(sizeof(int*) * numWorkers)), "malloc");
  //p = (int**)malloc(sizeof(int*) * numWorkers); //array delle pipe
  int *arrtmp; // = malloc(sizeof(int) * numWorkers);
  ec_null((arrtmp = malloc(sizeof(int) * numWorkers)), "malloc");
  for(int i = 0; i < numWorkers; i++) {
    //int *numthread = malloc(sizeof(int));
    //*numthread = &i;
    arrtmp[i] = i; //per passarlo al thread
    //int numthread = i;
    //fprintf(stderr, "sto passando i %d, numthread %d\n", i, numthread);
    if(pthread_create(&t[i], NULL, threadF, (void*)&(arrtmp[i])) != 0) { perror("pthread_sigmask"); exit(EXIT_FAILURE); }
    //dichiaro numWorkers pipe

    //int *pfd = malloc(sizeof(int) * 2);


    //int pfd[2];
    //if(pipe(pfd) == -1) { perror("pipe"); }

    //p[i] = (int*)malloc(sizeof(int) * 2);
    ec_null((p[i] = (int*)malloc(sizeof(int) * 2)), "malloc");

    ec_meno1((pipe(p[i])), "pipe");
    //if(pipe(p[i]) == -1) { perror("pipe"); }
    //close((p[i])[1]);
    //p[i] = pfd;
    //fprintf(stderr, "indirizzo pipe lettura %d scrittura %d\n", p[i][0], p[i][1]);
    //sleep(1);
  }


  int listenfd;
  int nattivi = 0; //numero connessioni attive

  //SYSCALL_EXIT("socket", listenfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
  listenfd = socket(AF_UNIX, SOCK_STREAM, 0);

  if(listenfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_un serv_addr;
  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sun_family = AF_UNIX;
  strncpy(serv_addr.sun_path, SockName, strlen(SockName)+1);

  int notused;
  SYSCALL_EXIT("bind", notused, bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "bind", "");
  //notused = bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr));
  SYSCALL_EXIT("listen", notused, listen(listenfd, MAXBACKLOG), "listen", "");
  //notused = listen(listenfd, MAXBACKLOG);

  fd_set tmpset; //fd che voglio aspettare
  // azzero sia il master set che il set temporaneo usato per la select
  //ec_meno1((FD_ZERO(&set)), "FD_ZERO");
  //ec_meno1((FD_ZERO(&tmpset)), "FD_ZERO");

  FD_ZERO(&set);
  FD_ZERO(&tmpset);

  // aggiungo il listener fd al master set
  FD_SET(listenfd, &set); //voglio ricevere informazioni da listenfd (socket principale)
  int fdmax = listenfd;

  for(int i = 0; i < numWorkers; i++) {
    FD_SET(p[i][0], &set); //p array di pipe, aggiungo tutte le pipe alla set in lettura
    if(p[i][0] > fdmax)
      fdmax = p[i][0];
    //fprintf(stderr, "inserisco il connfd della pipe %d\n", p[i][0]);
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
    //fprintf(stderr, "totale connessioni attive prima della select: %d\n", nattivi);
    /*if (select(fdmax+1, &tmpset, NULL, NULL, NULL) == -1) { //quando esco dalla select, sono sicuro che almeno uno dei file nella set ha qualcosa da leggere, sia accettare una nuova connessione che nuova richiesta dai client
      //in tmpset a questo punto abbiamo solo le cose effettivamente modificate
      perror("select");
      return -1;
    }*/
    ec_meno1((select(fdmax+1, &tmpset, NULL, NULL, NULL)), "select");

// cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
//fprintf(stderr, "fdmax %d\n", fdmax);
    for(int i = 0; i <= fdmax; i++) {
      //fprintf(stderr, "ciao7\n");
      if (FD_ISSET(i, &tmpset)) { //ora so che qualcosa è pronto, ma non so cosa, e devo capire se l'i-esimo è pronto
        long connfd;
        if (i == listenfd) { //SIGHUP CONTROL // e' una nuova richiesta di connessione
          //SYSCALL_EXIT("accept", connfd, accept(listenfd, (struct sockaddr*)NULL ,NULL), "accept", "");
          ec_meno1((connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)), "accept");
          //connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
          FD_SET(connfd, &set);  // aggiungo il descrittore al master set
          nattivi++;
          if(connfd > fdmax)
            fdmax = connfd;  // ricalcolo il massimo
          continue;
        }
        //ELSE
        connfd = i;  // e' una nuova richiesta da un client già connesso o da una pipe

        //gestione segnali
        if(connfd == psegnali[0]) { //se è vero, ho ricevuto un segnale
          fprintf(stderr, "Ho ricevuto segnale rsighup %d rsigint %d nel main\n", rsighup, rsigint);
          char buftmp[8];
          //read(connfd, buftmp, 7);
          SYSCALL_EXIT("readn", notused, readn(connfd, buftmp, 7), "read", "");
          if(rsigint == 1 || nattivi == 0) {
            rsigint = 1;
            if(pthread_cond_broadcast(&condQueueClient) != 0) { perror("pthread_sigmask"); exit(EXIT_FAILURE); }
          }
          //va gestito
          /*if(rsigint) { //il segnale ricevuto è SIGINT

          }*/
          if(rsighup) {
            FD_CLR(listenfd, &set);
            ec_meno1((close(listenfd)), "close");
            //close(listenfd);
          }
          continue;
        }

        //controllo se è una delle pipe
        int ispipe = 0;
        for(int j = 0; j < numWorkers; j++) {
          //fprintf(stderr, "sono nel for, connfd %ld p[j][0] %d\n", connfd, p[j][0]);
          if(connfd == p[j][0]) {
            //fprintf(stderr, "è una pipe\n");
            char buftmp[7];
            SYSCALL_EXIT("readn", notused, readn(connfd, buftmp, 6), "read", "");
            //read(connfd, buftmp, 6);
            //fprintf(stderr, "è una pipe\n");
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
        fprintf(stderr, "Sono il main, ho ricevuto una nuova richiesta da %ld\n", connfd);

        FD_CLR(connfd, &set);

        //msg_t str;
        char comando;
        int r = readn(connfd, &comando, sizeof(char));
        //int r = readn(connfd, &str.len, sizeof(int));
        if(r == 0 || r == -1) { //il socket è vuoto, connessione chiusa

          fprintf(stderr, "Client %ld disconnesso\n", connfd);
          nattivi--;
          FD_CLR(connfd, &set);
          //close(connfd);
          ec_meno1((close(connfd)), "close");
          if (connfd == fdmax)
            fdmax = updatemax(set, fdmax);
          if(nattivi > 0) {
            if(rsighup)
              fprintf(stderr, "ci sono connessioni attive, nattivi %d\n", nattivi);
            fprintf(stdout, "");
          } else {
            if(rsighup)
              fprintf(stderr, "Non ci sono altre connessioni\n");
            if(rsighup) {
              rsigint = 1; //a questo punto si deve comportare come avesse ricevuto un sigint
              if(pthread_cond_broadcast(&condQueueClient) != 0) { perror("pthread_cond_broadcast"); exit(EXIT_FAILURE); }
            }

          }
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
        if (r < 0) { perror("readn"); exit(EXIT_FAILURE); }
        char* str;
        int strlength;
        SYSCALL_EXIT("readn", notused, readn(connfd, &strlength, sizeof(int)), "read", "");

        //str.len = str.len - sizeof(char);
        //togliamo sizeof(char) perchè nella read al comando prima stiamo leggendo già un carattere
        //if (readn(connfd, &comando, sizeof(char))<=0) { fprintf(stderr, "sbagliato4\n"); }
        //SYSCALL_EXIT("readn", notused, readn(connfd, &comando, sizeof(char)), "read", "");


        //fprintf(stderr, "fin qui, ho letto la lunghezza %d\n", str.len);
        //str.str = calloc((str.len), sizeof(char));
        ec_null((str = calloc(strlength, sizeof(char))), "calloc");
        /*if (!str.str) {
    	     perror("calloc");
    	      fprintf(stderr, "Memoria esaurita....\n");
    	       //return NULL;
        }*/
        SYSCALL_EXIT("readn", notused, readn(connfd, str, strlength * sizeof(char)), "read", "");
        //if (readn(connfd, str.str, (str.len)*sizeof(char))<=0) { fprintf(stderr, "sbagliato2\n"); }
        //if (readn(connfd, str.str, (str.len - sizeof(char))*sizeof(char))<=0) { fprintf(stderr, "sbagliato2\n"); }
        //togliamo sizeof(char) perchè nella read al comando prima stiamo leggendo già un carattere
        //fprintf(stderr, "fin qui2222\n");

        //inserisco in coda il comando letto
        ComandoClient *cmdtmp; // = malloc(sizeof(ComandoClient));
        ec_null((cmdtmp = malloc(sizeof(ComandoClient))), "malloc");
        cmdtmp->comando = comando;
        ec_null((cmdtmp->parametro = malloc(sizeof(char) * (strlen(str) + 1))), "malloc");
        //cmdtmp->parametro = malloc(sizeof(char) * (strlen(str.str)) + 1);
        cmdtmp->connfd = connfd;
        strcpy(cmdtmp->parametro, str);
        //int xyz = strlen(str);
        cmdtmp->parametro[strlen(str)] = '\0';
        free(str);
        //fprintf(stderr, "il parametro è %s\n", cmdtmp->parametro);

        Pthread_mutex_lock(&mutexQueueClient);
        //fprintf(stderr, "parte1\n");
        //printQueueClient(queueClient);
        //fprintf(stderr, "parte32482\n");

        //push(&queueClient, cmdtmp);
        ec_meno1((push(&queueClient, cmdtmp)), "push");
        //fprintf(stderr, "parte2\n");

        Pthread_mutex_unlock(&mutexQueueClient);
        if(pthread_cond_signal(&condQueueClient) != 0) { perror("pthread_cond_signal"); exit(EXIT_FAILURE); }


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
  //cleanup
  //fprintf(stderr, "listenfd %d\n", listenfd);
  //aspetto che i thread terminino
  for(int i = 0; i < numWorkers; i++)
    if(pthread_join(t[i], NULL) != 0) { perror("pthread_sigmask"); exit(EXIT_FAILURE); }
  if(pthread_join(tGestoreSegnali, NULL) != 0) { perror("pthread_sigmask"); exit(EXIT_FAILURE); }

  //chiusura connessioni

  //chiusura connessioni pipe
  for(int i = 0; i < numWorkers; i++) {
    ec_meno1((close(p[i][0])), "close");
    ec_meno1((close(p[i][1])), "close");
    //close(p[i][0]);
    //close(p[i][1]);
    FD_CLR(p[i][0], &set);
    free(p[i]);
  }
  free(p);
  FD_CLR(psegnali[0], &set); //chiusura pipe segnali
  ec_meno1((close(psegnali[0])), "close");
  ec_meno1((close(psegnali[1])), "close");
  //close(psegnali[0]);
  //close(psegnali[1]);
  //FD_CLR(psegnali[0], &set); //chiusura pipe segnali
  free(psegnali);

  //chiusura connessioni attive
  for(int i = fdmax; i >= 0; --i) {
    if(FD_ISSET(i, &set)) {
      fprintf(stderr, "Sono il main, sto chiudendo la connessione %d\n", i);
      //close(i);
      ec_meno1((close(i)), "close");
    }
  }
  cleanup();
  //atexit(cleanup);

  //stampo le statistiche
  fprintf(stdout, "\n\nStatistiche del server raggiunte:\n");
  fprintf(stdout, "Numero massimo di file raggiunti: %d\n", s->numMaxMemorizzato);
  fprintf(stdout, "Numero massimo di spazio occupato: %.2f MB\n", s->spazioMaxOccupato / 1000000);
  fprintf(stdout, "Numero di volte che ho eseguito l'algoritmo di rimpiazzamento: %d\n", s->numAlgoRimpiazzamento);
  printQueueFiles(queueFiles);

  free(arrtmp); //free array numero thread
  free(t); //free array di thread
  free(s); //free statistiche

  //free della coda dei file
  fileRAM* tmpcodafile = pop(&queueFiles);
  while(tmpcodafile != NULL) {
    free(tmpcodafile->nome);
    free(tmpcodafile->buffer);
    free(tmpcodafile);
    tmpcodafile = pop(&queueFiles);
  }
  free(queueFiles);

  //free della coda dei client (nel caso del SIGINT/SIGQUIT)
  ComandoClient* tmpcodacomandi = pop(&queueClient);
  while(tmpcodacomandi != NULL) {
    free(tmpcodacomandi->parametro);
    free(tmpcodacomandi);
    tmpcodacomandi = pop(&queueClient);
  }
  free(queueClient);

  //fprintf(stderr, "sono fuori dal for\n");
  //sleep(3);
  //chiusura socket, pipe
}
