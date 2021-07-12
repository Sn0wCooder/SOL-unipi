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

#define MAXBUFFER 1000
#define MAXSTRING 100
#define CONFIGFILE "./configs/config.txt"
#define SPAZIO "spazio"
#define NUMEROFILE "numeroFile"
#define SOC "sockName"
#define WORK "numWorkers"
#define MAXBACKLOG 32
#define BYTETOMB 1048576

int spazio = 0; //spazio massimo del server
int numeroFile = 0; //numero file massimo del server
int numWorkers = 0; //numero thread workers
char* SockName = NULL; //nome del socket AF_UNIX a cui connettersi
Queue *queueClient; //coda dei comandi gestiti dai thread
Queue *queueFiles; //coda dei file memorizzati

int spazioOccupato = 0; //spazio occupato dal server

//statistiche
typedef struct _stats { //statistiche a fine esecuzione
  int numMaxMemorizzato;
  double spazioMaxOccupato;
  int numAlgoRimpiazzamento;
  pthread_mutex_t mutexStats;
} Statistiche;

Statistiche *s;

int rsigint; //se ho ricevuto un segnale SIGINT/SIGQUIT questa variabile viene settata a 1, utile per gestire i segnali
int rsighup; //idem per il segnale SIGHUP
int* psegnali; //pipe dei segnali, così che il thread dei segnali notifica al main la ricezione del segnale stesso

fd_set set; //set della select
int **p; //array delle pipe dei thread (così che possano notificare al main che hanno finito l'esecuzione di un comando, per svegliare la select)

static pthread_mutex_t mutexQueueClient = PTHREAD_MUTEX_INITIALIZER; //mutex della coda dei comandi dei client
static pthread_mutex_t mutexQueueFiles = PTHREAD_MUTEX_INITIALIZER; //mutex della coda dei file
pthread_cond_t condQueueClient = PTHREAD_COND_INITIALIZER; //per svegliare i thread quando viene ricevuto un nuovo comando

typedef struct _comandoclient { //un comando generico che deve essere processato da un thread
  char comando;
  char* parametro;
  long connfd;
} ComandoClient;

typedef struct _file { //un file generico memorizzato in memoria principale
  char* nome;
  char* buffer; //contenuto
  long length; //per fare la read e la write
  int is_locked; //-1 di default, quando faccio la openFile viene settato al connfd del client, quando faccio la closeFile viene resettato al valore di default
  pthread_mutex_t lock; //quando eseguo operazioni sul file faccio prima la lock, così altri client non mi vanno ad esempio a eliminare il file mentre lo sto usando
} fileRAM;

void cleanup() {
  if(SockName != NULL) {
    unlink(SockName);
    free(SockName);
  }
}

int updatemax(fd_set set, int fdmax) { //aggiorna la set e il suo nuovo massimo
  for(int i = (fdmax - 1);i >= 0; --i)
	 if (FD_ISSET(i, &set))
    return i;
    return -1;
}

void parser(char* configfile) { //parsa il file config.txt passato come primo argomento al server (e come input alla funzione)
  char* a = NULL;
  int i;
  char* save;
  char* token;
  char* buffer;
  ec_null((buffer = malloc(sizeof(char) * MAXBUFFER)), "malloc");
  FILE* p;
  ec_null((p = fopen(configfile, "r")), "fopen"); //apre il file config.txt
  while(fgets(buffer, MAXBUFFER, p)) { //per ogni riga di questo file
    //parser della singola riga
    save = NULL;
    buffer[strlen(buffer) - 1] = '\0'; //inserisco il terminatore per evitare il newline finale
    token = strtok_r(buffer, " ",  &save); //tokenizzo in base allo spaxio
    char** tmp;
    ec_null((tmp = malloc(sizeof(char*) * 2)), "malloc"); //tmp[0]: stringa con il nome, tmp[1]: stringa con il valore associato
    i = 0;
    while(token) {
      ec_null((tmp[i] = malloc(sizeof(char) * MAXSTRING)), "malloc");
      strcpy(tmp[i], token); //se è il secondo elemento (i = i) non deve prendere il newline finale,
      token = strtok_r(NULL, " ", &save);
      i++;
    }

    if(strcmp(tmp[0], SPAZIO) == 0) { //spazio massimo da mettere nel server
      if(isNumber(tmp[1])) {
        spazio = atoi(tmp[1]);
      } else {
        fprintf(stderr, "Errato config.txt (isNumber)\n");
        exit(EXIT_FAILURE);
      }
    }
    if(strcmp(tmp[0], NUMEROFILE) == 0) { //numero file massimo da memorizzare
      if(isNumber(tmp[1])) {
        numeroFile = atoi(tmp[1]);
      } else {
        fprintf(stderr, "Errato config.txt (isNumber)\n");
        exit(EXIT_FAILURE);
      }
    }
    if(strcmp(tmp[0], SOC) == 0) { //sockName AF_UNIX a cui connettersi
      ec_null((SockName = malloc(sizeof(char) * (strlen(tmp[1]) + 1))), "malloc");
      strcpy(SockName, tmp[1]);
      SockName[strlen(tmp[1])] = '\0';
    }
    if(strcmp(tmp[0], WORK) == 0) { //numero thread workers
      if(isNumber(tmp[1])) {
        numWorkers = atoi(tmp[1]);
      } else {
        fprintf(stderr, "Errato config.txt (isNumber)\n");
        exit(EXIT_FAILURE);
      }
    }

    free(tmp[0]);
    free(tmp[1]);
    free(tmp);
  }
  if(fclose(p) != 0) { perror("fclose"); exit(EXIT_FAILURE); } //chiude il file



  free(buffer);

  if(spazio <= 0 || numeroFile <= 0 || SockName == NULL || numWorkers <= 0) { //errore nel file di configurazione
    fprintf(stderr, "config.txt errato\n");
    exit(EXIT_FAILURE);
  }
  float sptmp = spazio / BYTETOMB;
  fprintf(stdout,"Spazio: %.2f MB\n", sptmp);
  fprintf(stdout,"numeroFile: %d\n", numeroFile);
  fprintf(stdout,"SockName: %s\n", SockName);
  fprintf(stdout,"numWorkers: %d\n", numWorkers);
  //}
}

void printQueueFiles(Queue *q) { //stampa la coda dei file
  Node* tmp = q->head;
  fileRAM *no = NULL;
  float spaziotmp = spazioOccupato; //spazio fino ad ora occupato in float (per convertirlo in MB)
  spaziotmp/=BYTETOMB;
  fprintf(stdout, "Coda dei file (spazio occupato %.2f MB):\n", spaziotmp);
  while(tmp != NULL) { //per ogni elemento della coda
    no = tmp->data;
    //float nosptmp = no->length / BYTETOMB;
    fprintf(stdout, "File name %s, size %ld bytes\n", no->nome, no->length);
    tmp = tmp->next;
  }
  fprintf(stdout, "Fine stampa coda dei file\n");
}

Node* fileExistsInServer(Queue *q, char* nomefile) { //se esiste il file nel server restituisce il Node* associato, altrimenti ritorna NULL
  if(q == NULL || nomefile == NULL) {
    errno = EINVAL;
    perror("fileExistsInServer");
    exit(EXIT_FAILURE);
  }
  Node* tmp = q->head;
  fileRAM *no = NULL;
  while(tmp != NULL) { //controlla ogni file
    no = tmp->data;
    if(strcmp(basename(nomefile), no->nome) == 0) {
      return tmp;
    }
    tmp = tmp->next;
  }
  return NULL;
}

static void* threadF(void* arg) { //thread worker
  int numthread = *(int*)arg; //numero del thread passato come argomento alla funzione
  fprintf(stdout, "Sono il thread %d\n", numthread);
  while(1) { //ciclo infinito: finchè gli arrivano cose da fare va avanti. Si blocca solo quando riceve un segnale, nel break più sotto
    Pthread_mutex_lock(&mutexQueueClient);
    while(queueClient->len == 0 && !rsigint) { //finchè la lunghezza è 0 e non ho ricevuto un segnale SIGINT
      if(pthread_cond_wait(&condQueueClient, &mutexQueueClient) != 0) { perror("pthread_cond_wait"); exit(EXIT_FAILURE); } //aspetto il main
      fprintf(stdout, "Sono sveglio (num thread %d)!\n", numthread);
      }

    //gestione segnali
    if(rsigint == 1) {
      Pthread_mutex_unlock(&mutexQueueClient);
      fprintf(stderr, "Sono il thread %d e ho ricevuto un segnale\n", numthread);
      break; //esce dal while(1)
    }

    ComandoClient* tmp = pop(&queueClient); //estrae il primo comando nella coda
    Pthread_mutex_unlock(&mutexQueueClient);


    long connfd = tmp->connfd; //connfd del client che ha fatto la richiesta
    char comando = tmp->comando; //comando da eseguire
    char* parametro = tmp->parametro; //parametro (generalmente il pathname del file)

    fprintf(stdout, "Sto eseguendo il comando %c che ha parametro %s del connfd %ld\n", comando, parametro, connfd);
    int notused; //per la SYSCALL_EXIT

    //POSSIBILI COMANDI PASSATI
    switch(comando) {
      case 'W': { //richiesta di scrittura di un file
        fprintf(stdout, "Ho ricevuto un comando di scrittura del file %s\n", parametro);
        Pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro); //controlla che il file esista all'interno del server. Infatti prima va fatta la openFile con il flag O_CREATE
        Pthread_mutex_unlock(&mutexQueueFiles);

        int ris = 0;
        if(esiste == NULL) { //errore: il file non esiste (dovrebbe già essere stato creato dalla openFile)
          ris = -1;
          SYSCALL_EXIT("writen", notused, writen(connfd, &ris, sizeof(int)), "write", "");
        } else { //file esiste
          fileRAM *newfile = esiste->data;
          Pthread_mutex_lock(&newfile->lock);
          if(newfile->is_locked != connfd) { //file eistse ma non aperto dal client
            ris = -1;
          }
          SYSCALL_EXIT("writen", notused, writen(connfd, &ris, sizeof(int)), "write", ""); //scrive se fino ad ora può andare avanti

          if(ris != -1) { //può andare avanti, fin qui tutto ok
            int lentmp; //lunghezza del file che deve scrivere
            SYSCALL_EXIT("readn", notused, readn(connfd, &lentmp, sizeof(int)), "readn", ""); //legge la lunghezza sopra citata
            int cista = 1; //ci sta (come spazio) nel server fino a prova contraria
            if(lentmp > spazio || lentmp + newfile->length > spazio) { //non lo memorizza proprio, è troppo grande anche se il server non avesse file memorizzati
              //il secondo controllo dopo l'or è per vedere se l'append di un file non ci sta nel server
              fprintf(stderr, "Il file %s è troppo grande (%ld bytes) e non sta materialmente nel server (capienza massima %d bytes)\n", newfile->nome, lentmp + newfile->length, spazio);

              if(lentmp > spazio) //se il file non esiste ancora (ha size null)
                removeFromQueue(&queueFiles, esiste); //lo rimuovo dalla coda

              free(newfile->nome);
              free(newfile);
              cista = 0; //setto il flag a 0: il file non ci sta
            }
            //vado a scrivere nel socket se il file ci sta
            SYSCALL_EXIT("writen", notused, writen(connfd, &cista, sizeof(int)), "write", "");

            if(cista) { //se il file ci sta deve memorizzarlo
              fileRAM *fileramtmptrash;
              Pthread_mutex_lock(&mutexQueueFiles);

              //aggiorno le statistiche
              Pthread_mutex_lock(&s->mutexStats);
              if(spazioOccupato + lentmp > spazio)
                s->numAlgoRimpiazzamento++;
              Pthread_mutex_unlock(&s->mutexStats);

              while(spazioOccupato + lentmp > spazio) { //algoritmo di rimpiazzamento: se la condizione è vera deve iniziare ad espellere file
                float spaziotmp = spazioOccupato; //per stamparlo in MB
                spaziotmp/=BYTETOMB;
                float lentmptmp = lentmp; //per stamparlo in MB
                lentmptmp/=BYTETOMB;
                fprintf(stderr, "Il server è pieno (di spazio, spazio occupato %.2f MB, da inserire %.2f MB). Devo espellere file.\n", spaziotmp, lentmptmp);
                fileRAM *firstel = returnFirstEl(queueFiles); //ritorna il primo elemento dalla lista
                if(newfile != firstel) { //non deve rimuove il primo elemento perchè è proprio quello che dobbiamo inserire
                  fileramtmptrash = pop(&queueFiles);
                } else { //rimuove il secondo elemento
                  fileramtmptrash = pop2(&queueFiles);
                }
                fprintf(stderr, "Sto espellendo il file %s dal server per far spazio al file %s\n", fileramtmptrash->nome, parametro);
                spazioOccupato-=fileramtmptrash->length; //libero lo spazio dal file eliminato
                free(fileramtmptrash->nome);
                if(fileramtmptrash->buffer != NULL) //il buffer potrebbe non esistere
                  free(fileramtmptrash->buffer);
                free(fileramtmptrash);
              }

              //arrivati a questo punto, il file ci sta e deve essere (finalmente) memorizzato
              spazioOccupato+=lentmp; //incremeneta lo spazio occupato
              Pthread_mutex_unlock(&mutexQueueFiles);

              char* buftmp; //buffer del file da memorizzare
              ec_null((buftmp = malloc(sizeof(char) * lentmp)), "malloc");

              SYSCALL_EXIT("readn", notused, readn(connfd, buftmp, lentmp * sizeof(char)), "read", ""); //legge il buffer

              //ora dobbiamo fare la scrittura in memoria principale
              if(newfile->buffer == NULL) { //file vuoto, prima scrittura
                newfile->length = lentmp;
                newfile->buffer = buftmp;
              } else { //file già pieno, appendToFile
                fprintf(stderr, "Scrittura in append\n");
                ec_null((newfile->buffer = realloc(newfile->buffer, sizeof(char) * (lentmp + newfile->length))), "realloc"); //rialloca il buffer come dimensione quella del nuov file
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

              printQueueFiles(queueFiles); //stampa la coda dei file dopo ogni scrittura

              SYSCALL_EXIT("writen", notused, writen(connfd, &ris, sizeof(int)), "write", ""); //scrive il risultato da mandare al client
            }
          }
          Pthread_mutex_unlock(&newfile->lock);
        }


        break;
      }
      case 'c': { //cancellazione di un file
        fprintf(stdout, "Ho ricevuto un comando di cancellazione del file %s\n", parametro);
        Pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro); //nodo da cancellare

        int risposta;
        if(esiste == NULL) { //file non esiste
          Pthread_mutex_unlock(&mutexQueueFiles);
          fprintf(stderr, "Errore: file %s NON rimosso dal server (non esisteva)\n", parametro);
          risposta = -1;
        } else { //file esiste
          fileRAM *tmpfileramtrash = esiste->data; //fileRAM associato al nodo
          if(tmpfileramtrash->is_locked != connfd) { //file esiste ma non lockato
            fprintf(stderr, "Errore: file %s non rimosso dal server perchè non aperto dal client\n", parametro);
            risposta = -1;
          } else { //file esiste e lockato, deve essere rimosso
            spazioOccupato-= tmpfileramtrash->length; //aggiorno lo spazio occupato
            risposta = removeFromQueue(&queueFiles, esiste); //rimozione dalla coda
            free(tmpfileramtrash->nome);
            if(tmpfileramtrash->buffer != NULL)
              free(tmpfileramtrash->buffer);
            free(tmpfileramtrash);
            fprintf(stdout, "File %s rimosso con successo dal server\n", parametro);
          }
          Pthread_mutex_unlock(&mutexQueueFiles);

        }
        printQueueFiles(queueFiles); //stampo la coda dei file dopo ogni cancellazione
        SYSCALL_EXIT("writen", notused, writen(connfd, &risposta, sizeof(int)), "write", ""); //mando il risultato dell'operazione al client
        break;
      }
      case 'r': { //lettura di un file
        fprintf(stdout, "Ho ricevuto un comando di lettura del file %s\n", parametro);
        Pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro); //nodo associato al fileRAM da leggere
        int len; //funge anche da risposta che ci dice se è tutto ok o no
        if(esiste != NULL) { //file esistente nel server
          fileRAM *filetmp = esiste->data;
          Pthread_mutex_lock(&filetmp->lock);
          Pthread_mutex_unlock(&mutexQueueFiles);
          if(filetmp->is_locked == connfd) { //file aperto da quel client
            len = filetmp->length;
            char* buf = filetmp->buffer;
            SYSCALL_EXIT("writen", notused, writen(connfd, &len, sizeof(int)), "write", ""); //mando la lunghezza al client
            SYSCALL_EXIT("writen", notused, writen(connfd, buf, len * sizeof(char)), "write", ""); //mando il buffer al client
            fprintf(stdout, "File %s letto con successo dal server\n", parametro);
          } else { //file non aperto da quel client
            len = -1;
            SYSCALL_EXIT("writen", notused, writen(connfd, &len, sizeof(int)), "write", ""); //scrivo la risposta (errore)
            fprintf(stderr, "Errore: file %s NON letto dal server perchè non aperto dal client\n", parametro);
          }
          Pthread_mutex_unlock(&filetmp->lock);
        } else { //ERRORE: file non trovato nel server
          Pthread_mutex_unlock(&mutexQueueFiles);
          len = -1;
          SYSCALL_EXIT("writen", notused, writen(connfd, &len, sizeof(int)), "write", ""); //scrivo la risposta (errore)
          fprintf(stderr, "Errore: file %s NON letto dal server (non esisteva)\n", parametro);
        }
        break;
      }
      case 'R': { //lettura di N file dal server
        fprintf(stderr, "Ho ricevuto un comando readNFiles con n = %d\n", atoi(parametro));
        int numDaLeggere; //numero di file da leggere

        if(atoi(parametro) > queueFiles->len) //troppi file da leggere, più di quanti sono memorizzati nel server
          numDaLeggere = queueFiles->len;
        else
          numDaLeggere = atoi(parametro);
        if(numDaLeggere <= 0) //leggo tutti i file
          numDaLeggere = queueFiles->len;
        SYSCALL_EXIT("writen", notused, writen(connfd, &numDaLeggere, sizeof(int)), "write", ""); //scrivo il numero di file che il client può effettivamente leggere
        Node* nodetmp = queueFiles->head;
        fileRAM *fileramtmp;
        char* buftmp;
        for(int i = 0; i < numDaLeggere; i++) { //per ogni file da leggere
          fileramtmp = nodetmp->data;
          ec_null((buftmp = malloc(sizeof(char) * (strlen(fileramtmp->nome) + 1))), "malloc");
          strcpy(buftmp, fileramtmp->nome);
          int buftmplen = strlen(buftmp);
          //per ogni file da leggere, il server manda al client il suo nome
          SYSCALL_EXIT("writen", notused, writen(connfd, &buftmplen, sizeof(int)), "write", "");
          SYSCALL_EXIT("writen", notused, writen(connfd, buftmp, buftmplen * sizeof(char)), "write", "");
          free(buftmp);
          nodetmp = nodetmp->next;
        }
        break;
      }
      case 'o': { //openFile
        fprintf(stderr, "Ho ricevuto un comando di apertura del file %s nel server\n", parametro);
        int risposta;
        Pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro);
        int flags; //flag passati dal client
        SYSCALL_EXIT("readn", notused, readn(connfd, &flags, sizeof(int)), "read", ""); //leggo i flag che deve usare la open
        if(esiste == NULL && flags == 0) { //deve aprire il file ma non esiste, errore
          risposta = -1;
        } else if(esiste == NULL && flags == 1) { //deve creare e aprire il file (che non esiste)
          if(queueFiles->len + 1 > numeroFile) { //deve iniziare ad espellere file, algoritmo di rimpiazzamento (per numero dei file)
            fprintf(stdout, "Il server è pieno (di numero file), ne cancello uno\n");
            fileRAM *fileramtmptrash = pop(&queueFiles);
            fprintf(stdout, "Sto espellendo il file %s dal server\n", fileramtmptrash->nome);
            spazioOccupato-=fileramtmptrash->length; //aggiorna lo spazio occupato
            free(fileramtmptrash->nome);
            free(fileramtmptrash->buffer);
            free(fileramtmptrash);
          }

          fileRAM *newfile; //nuovo file da inserire
          ec_null((newfile = malloc(sizeof(fileRAM))), "malloc");
          //inizializza i dati di quel file
          if(pthread_mutex_init(&newfile->lock, NULL) != 0) { perror("pthread_mutex_init"); exit(EXIT_FAILURE); } //inizializza la lock di quel file
          ec_null((newfile->nome = malloc(sizeof(char) * (strlen(basename(parametro)) + 1))), "malloc");
          strcpy(newfile->nome, basename(parametro));
          newfile->nome[strlen(basename(parametro))] = '\0';
          newfile->length = 0;
          newfile->buffer = NULL;
          newfile->is_locked = connfd;
          ec_meno1((push(&queueFiles, newfile)), "push");
          risposta = 0; //tutto ok
        }
        else if(esiste != NULL && flags == 0) { //deve aprire il file, che esiste già
          fileRAM *fileramtmp = esiste->data; //controlla se il file esiste
          if(fileramtmp->is_locked != -1)
            risposta = -1; //errore: il file non esiste
          else {
            fileramtmp->is_locked = connfd; //'locka' il file con il connfd del client
            risposta = 0;
          }
        } else if(esiste != NULL && flags == 1) { //deve creare e aprire il file, che esiste già, errore
          risposta = -2; //errore: crea un file ma esiste già. Il client può riprovare a crearlo con il flag O_LOCK al posto di O_CREATE
        }
        Pthread_mutex_unlock(&mutexQueueFiles);
        SYSCALL_EXIT("writen", notused, writen(connfd, &risposta, sizeof(int)), "write", ""); //mando al client la risposta della openFile
        if(risposta == 0)
          fprintf(stdout, "Ho aperto con successo il file %s\n", parametro);
        else
          fprintf(stderr, "Errore: non ho aperto il file %s, codice errore %d\n", parametro, risposta);
        break;
      }
      case 'z': { //closeFile
        int ris; //da mandare al client
        Pthread_mutex_lock(&mutexQueueFiles);
        Node* esiste = fileExistsInServer(queueFiles, parametro); //nodo che deve esistere all'interno del server
        if(esiste == NULL) { //file da chiudere non esiste nel server
          Pthread_mutex_unlock(&mutexQueueFiles);
          ris = -1; //errore
        } else { //file da chiudere esiste nel server
          fileRAM *fileramtmp = esiste->data;
          Pthread_mutex_lock(&fileramtmp->lock);
          if(fileramtmp->is_locked != connfd) { //errore: file non aperto da quel client
            ris = -1; //un client prova a chiudere un file ma non lo aveva aperto lui, errore
          } else { //chiudo il file
            fileramtmp->is_locked = -1; //resetta il flag is_locked al valore di default (che significa che il file non è aperto)
            ris = 0; //tutto ok
          }
          Pthread_mutex_unlock(&fileramtmp->lock);
          Pthread_mutex_unlock(&mutexQueueFiles);
        }
        SYSCALL_EXIT("writen", notused, writen(connfd, &ris, sizeof(int)), "write", ""); //mando al client la risposta della closeFile
        break;
      }
    }

    FD_SET(connfd, &set); //riaggiungo il connfd del client alla set, così che possa mandare altre richieste al server
    SYSCALL_EXIT("writen", notused, writen(p[numthread][1], "finito", 6), "write", ""); //scrivo all'interno della pipe per svegliare la select nel main

    free(tmp->parametro);
    free(tmp);
  }
  //uscita dal main del thread
  fprintf(stderr, "Sto uscendo dal thread %d\n", numthread);
  return NULL;
}

void printQueueClient(Queue *q) { //stampa la coda dei comandi
  Node* tmp = q->head;
  ComandoClient *no = NULL;
  while(tmp != NULL) {
    no = tmp->data;
    fprintf(stdout, "parametro %s comando %c connfd %ld\n", no->parametro, no->comando, no->connfd);
    tmp = tmp->next;
  }
}

static void* tSegnali(void* arg) { //thread dei segnali
  if(arg == NULL) {
    errno = EINVAL;
    perror("tSegnali");
    exit(EXIT_FAILURE);
  }
  //control + C = segnale 2, SIGINT
  //control + \ = segnale 3, SIGQUIT
  //SIGINT uguale a SIGQUIT
  //kill -1 PID = segnale 1, SIGHUP

  sigset_t *mask = (sigset_t*)arg; //maschera per la sigwait (dichiarata nel main)
  fprintf(stdout, "Sono il thread gestore segnali\n");
  int segnalericevuto;
  sigwait(mask, &segnalericevuto); //aspetta un segnale
  fprintf(stdout, "Sono il thread gestore segnali, ho ricevuto il segnale %d\n", segnalericevuto);
  if(segnalericevuto == SIGINT || segnalericevuto == SIGQUIT) { //gestione SIGINT o SIGQUIT, che si controllano allo stesso modo
    rsigint = 1;
  } else if(segnalericevuto == SIGHUP) { //gestione SIGHUP
    rsighup = 1;
  } /*else {
    tSegnali(arg);
    return NULL;
  }*/
  int notused;
  SYSCALL_EXIT("writen", notused, writen(psegnali[1], "segnale", 7), "write", ""); //scrive nella pipe per svegliare la select nel main
  fprintf(stdout, "Sono il thread gestore segnali e ho concluso ciò che dovevo fare\n"); //esce dal thread
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
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGQUIT);
  sigaddset(&mask, SIGINT);
  if(pthread_sigmask(SIG_SETMASK, &mask, NULL) != 0) { perror("pthread_sigmask"); exit(EXIT_FAILURE); }
  pthread_t tGestoreSegnali; //inizializzo il thread
  if(pthread_create(&tGestoreSegnali, NULL, tSegnali, (void*)&mask) != 0) { perror("pthread_sigmask"); exit(EXIT_FAILURE); } //creo il thread
  ec_null((psegnali = (int*)malloc(sizeof(int) * 2)), "malloc"); //pipe dei segnali
  ec_meno1((pipe(psegnali)), "pipe");

  queueClient = initQueue(); //inizializzo la coda dei file descriptor dei client che provano a connettersi
  queueFiles = initQueue(); //inizializzo la coda dei files

  pthread_t *t; //array dei thread workers
  ec_null((t = malloc(sizeof(pthread_t) * numWorkers)), "malloc");
  ec_null((p = (int**)malloc(sizeof(int*) * numWorkers)), "malloc"); //array delle pipe associate ad ogni thread
  int *arrtmp;
  ec_null((arrtmp = malloc(sizeof(int) * numWorkers)), "malloc"); //array per passare il numero al thread
  for(int i = 0; i < numWorkers; i++) {
    arrtmp[i] = i; //per passarlo al thread
    if(pthread_create(&t[i], NULL, threadF, (void*)&(arrtmp[i])) != 0) { perror("pthread_sigmask"); exit(EXIT_FAILURE); } //creo il thread worker
    ec_null((p[i] = (int*)malloc(sizeof(int) * 2)), "malloc"); //alloco la pipe
    ec_meno1((pipe(p[i])), "pipe"); //creo la pipe
  }


  int listenfd; //socket fd
  int nattivi = 0; //numero connessioni attive

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
  SYSCALL_EXIT("listen", notused, listen(listenfd, MAXBACKLOG), "listen", "");

  fd_set tmpset; //fd che voglio aspettare
  // azzero sia il master set che il set temporaneo usato per la select
  FD_ZERO(&set);
  FD_ZERO(&tmpset);

  // aggiungo il listener fd al master set
  FD_SET(listenfd, &set); //voglio ricevere informazioni da listenfd (socket principale)
  int fdmax = listenfd; //abbiamo al momento un solo fd

  for(int i = 0; i < numWorkers; i++) { //aggiungo le pipe alla set
    FD_SET(p[i][0], &set); //p array di pipe, aggiungo tutte le pipe alla set in lettura
    if(p[i][0] > fdmax)
      fdmax = p[i][0];
  }

  //inserisco la pipe dei segnali nella set
  FD_SET(psegnali[0], &set);
  if(psegnali[0] > fdmax)
    fdmax = psegnali[0];

  while(!rsigint) { //è un while(1), quando ricevo un segnale si blocca
    tmpset = set; // copio il set nella variabile temporanea per la select
    ec_meno1((select(fdmax+1, &tmpset, NULL, NULL, NULL)), "select");

    for(int i = 0; i <= fdmax; i++) { // cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
      if (FD_ISSET(i, &tmpset)) { //ora so che qualcosa è pronto, ma non so cosa, e devo capire se l'i-esimo è pronto
        long connfd;
        if (i == listenfd) { // e' una nuova richiesta di connessione dal socket
          ec_meno1((connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)), "accept"); //accetto la nuova richiesta
          FD_SET(connfd, &set); //aggiungo il descrittore al master set
          nattivi++;
          if(connfd > fdmax)
            fdmax = connfd;  //ricalcolo il massimo
          continue;
        }
        //ELSE
        connfd = i;  //è una nuova richiesta da un client già connesso o da una pipe

        //gestione segnali
        if(connfd == psegnali[0]) { //se è vero, ho ricevuto un segnale
          fprintf(stderr, "Ho ricevuto segnale rsighup %d rsigint %d nel main\n", rsighup, rsigint);
          char buftmp[8];
          SYSCALL_EXIT("readn", notused, readn(connfd, buftmp, 7), "read", "");
          if(rsigint == 1 || nattivi == 0) { //il SIGHUP è uguale al SIGINT, deve terminare
            rsigint = 1; //il SIGHUP si comporta come il SIGINT
            if(pthread_cond_broadcast(&condQueueClient) != 0) { perror("pthread_sigmask"); exit(EXIT_FAILURE); } //sveglia i thread
          }

          if(rsighup) { //ha ricevuto un segnale SIGHUP, non deve più accettare connessioni dal socket
            FD_CLR(listenfd, &set);
            ec_meno1((close(listenfd)), "close");
          }
          continue;
        }

        //controllo se è una delle pipe
        int ispipe = 0;
        for(int j = 0; j < numWorkers; j++) {
          if(connfd == p[j][0]) {
            char buftmp[7];
            SYSCALL_EXIT("readn", notused, readn(connfd, buftmp, 6), "read", ""); //leggo dalla pipe
            ispipe = 1;
          }
        }
        if(ispipe)
          continue;

        //altrimenti ha ricevuto un nuovo comando da un client
        fprintf(stderr, "Sono il main, ho ricevuto una nuova richiesta da %ld\n", connfd);

        FD_CLR(connfd, &set); //rimuove il connfd del client dalla set, perchè deve gestirlo il worker

        char comando;
        int r = readn(connfd, &comando, sizeof(char)); //lettura di un char: il comando che deve eseguire
        if(r == 0 || r == -1) { //il socket è vuoto, connessione chiusa

          fprintf(stderr, "Client %ld disconnesso\n", connfd);
          nattivi--;
          FD_CLR(connfd, &set);
          ec_meno1((close(connfd)), "close");
          if (connfd == fdmax)
            fdmax = updatemax(set, fdmax);
          if(nattivi > 0) { //controlla se ha ricevuto un segnale SIGHUP: se non ci sono altri client attivi e ha ricevuto SIGHUP si comporta come se avesse ricevuto SIGINT
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
          continue;
        }
        if (r < 0) { perror("readn"); exit(EXIT_FAILURE); }
        char* str;
        int strlength;
        SYSCALL_EXIT("readn", notused, readn(connfd, &strlength, sizeof(int)), "read", ""); //legge la lunghezza del parametro

        ec_null((str = calloc(strlength, sizeof(char))), "calloc");
        SYSCALL_EXIT("readn", notused, readn(connfd, str, strlength * sizeof(char)), "read", ""); //legge il parametro

        //inserisco in coda il comando letto
        ComandoClient *cmdtmp; //creo un nuovo comando, lo alloco e lo inizializzo
        ec_null((cmdtmp = malloc(sizeof(ComandoClient))), "malloc");
        cmdtmp->comando = comando;
        ec_null((cmdtmp->parametro = malloc(sizeof(char) * (strlen(str) + 1))), "malloc");
        cmdtmp->connfd = connfd;
        strcpy(cmdtmp->parametro, str);
        cmdtmp->parametro[strlen(str)] = '\0';
        free(str);
        Pthread_mutex_lock(&mutexQueueClient);
        ec_meno1((push(&queueClient, cmdtmp)), "push"); //lo inserisco in coda
        Pthread_mutex_unlock(&mutexQueueClient);
        if(pthread_cond_signal(&condQueueClient) != 0) { perror("pthread_cond_signal"); exit(EXIT_FAILURE); } //sveglio eventuali thread in attesa
      }
    }
  }
  //cleanup

  //aspetto che i thread terminino
  for(int i = 0; i < numWorkers; i++)
    if(pthread_join(t[i], NULL) != 0) { perror("pthread_sigmask"); exit(EXIT_FAILURE); }
  if(pthread_join(tGestoreSegnali, NULL) != 0) { perror("pthread_sigmask"); exit(EXIT_FAILURE); }

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
  free(psegnali);

  //chiusura connessioni attive
  for(int i = fdmax; i >= 0; --i) {
    if(FD_ISSET(i, &set)) {
      fprintf(stderr, "Sono il main, sto chiudendo la connessione %d\n", i);
      ec_meno1((close(i)), "close");
    }
  }
  cleanup();

  //stampo le statistiche
  fprintf(stdout, "\n\nStatistiche del server prima della chiusura:\n");
  fprintf(stdout, "Numero massimo di file raggiunti: %d\n", s->numMaxMemorizzato);
  fprintf(stdout, "Spazio occupato al massimo: %.2f MB\n", s->spazioMaxOccupato / BYTETOMB);
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

  return 0;
}
