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
#include <fcntl.h>
#include <signal.h>

#include <sys/un.h>
#include <ctype.h>

#include "queue.h"
#include "util.h"
#include "parser.h"

#define SOCKNAME "./configs/sock.sk"
#define MAXBUFFER 1000
#define O_CREATE 1 //flag per la openFile
#define O_OPEN 0 //flag per la openFile
long sockfd; //fd del socket a cui connettersi

static int add_to_current_time(long sec, long nsec, struct timespec* res){ //aggiunge al res i secondi su cui connettersi
  clock_gettime(CLOCK_REALTIME, res);
  res->tv_sec += sec;
  res->tv_nsec += nsec;
  return 0;
}


int set_timespec_from_msec(long msec, struct timespec* req) { //tempo di attesa per la openConnection
  if(msec < 0 || req == NULL){
    errno = EINVAL;
    return -1;
  }
  req->tv_sec = msec / 1000;
  msec = msec % 1000;
  req->tv_nsec = msec * 1000;
  return 0;
}



int openConnection(const char* sockname, int msec, const struct timespec abstime) { //apre la connessione
  if(sockname == NULL || msec < 0) { //argomenti non validi
    errno = EINVAL;
    return -1;
  }
  struct sockaddr_un serv_addr;
  SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
  memset(&serv_addr, '0', sizeof(serv_addr));

  serv_addr.sun_family = AF_UNIX;
  strncpy(serv_addr.sun_path, socknameconfig, strlen(socknameconfig) + 1);
  serv_addr.sun_path[strlen(socknameconfig)] = '\0';
  struct timespec wait_time; //tempo di attesa
  set_timespec_from_msec(msec, &wait_time);

  struct timespec curr_time;
  clock_gettime(CLOCK_REALTIME, &curr_time); //tempo corrente

  int err = -1;
  while((err = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) == -1 && curr_time.tv_sec < abstime.tv_sec) {

    if(nanosleep(&wait_time, NULL) == -1) {
      sockfd = -1;
      return -1;
    }
    if(clock_gettime(CLOCK_REALTIME, &curr_time) == -1) {
        sockfd = -1;
        return -1;
    }
  }

  if(err == -1) {

    sockfd = -1;
    errno = ETIMEDOUT;
    return -1;
  }
  if(verbose)
    fprintf(stdout, "Mi sono connesso al server!\n");
  return 0;
}

int closeConnection(const char* sockname) { //chiude la connessione con il server
  if(sockname == NULL){
    errno = EINVAL;
    return -1;
  }
  if(strcmp(sockname, socknameconfig) != 0) { //sock name sbagliato
    errno = ENOTCONN;
    return -1;
  }

  if(close(sockfd) == -1){
    sockfd = -1;
    return -1;
  }

  if(verbose)
    fprintf(stdout, "Connessione col server chiusa\n");

  sockfd = -1;
  return 0;
}

int writeCMD(const char* pathname, char cmd) { //scrive il comando nel server (che verrà catturato dal main del server)
  if(pathname == NULL){
    errno = EINVAL;
    return -1;
  }
  int notused;
  char* towrite; //parametro da scriver
  ec_null((towrite = malloc(sizeof(char) * strlen(pathname) + 1)), "malloc");
  strcpy(towrite, pathname);
  towrite[strlen(pathname)] = '\0';
  SYSCALL_EXIT("writen", notused, writen(sockfd, &cmd, sizeof(char)), "write", ""); //scrive il primo carattere, il char del comando da eseguire
  int n = strlen(towrite) + 1;
  //scrive il parametro da eseguire
  SYSCALL_EXIT("writen", notused, writen(sockfd, &n, sizeof(int)), "write", "");
  SYSCALL_EXIT("writen", notused, writen(sockfd, towrite, n * sizeof(char)), "write", "");
  free(towrite);
  return 0;
}

int closeFile(const char* pathname) { //resetta il flag is_locked nel server a -1
  if(pathname == NULL) {
    errno = EINVAL;
    return -1;
  }

  if(writeCMD(pathname, 'z') == -1) { //scrive il comando da eseguire
    errno = EPERM;
    return -1;
  }
  int res, notused;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &res, sizeof(int)), "read", ""); //legge la risposta
  if(res == -1) {
    errno = EPERM;
    fprintf(stderr, "closeFile di %s fallita\n", pathname);
  } else { //res = 0
    if(verbose)
      fprintf(stdout, "Ho fatto la closeFile di %s\n", pathname);
  }
  return res;
}

int openFile(const char* pathname, int flags) { //apre il file pathname nel server
  if((flags != 0 && flags != 1) || pathname == NULL) { // i flag passate non sono valide (0 -> open | 1 -> open & create)
    errno = EINVAL;
    return -1;
  }
  if(writeCMD(pathname, 'o') == -1) { //scrive il comando nel socket
    errno = EPERM;
    return -1;
  }
  int res, notused;
  SYSCALL_EXIT("writen", notused, writen(sockfd, &flags, sizeof(int)), "write", ""); //scrive i flag da usare nel socket
  SYSCALL_EXIT("readn", notused, readn(sockfd, &res, sizeof(int)), "read", "");
  if(res == 0) {
    if(verbose)
      fprintf(stdout, "Ho fatto la openFile di %s\n", pathname);
  } else if (res == -2) {
    fprintf(stderr, "File %s già esistente nel server (openFile fallita)\n", pathname);
    res = -1;
  } else { //res = -1, errore
    errno = EACCES;
    fprintf(stderr, "openFile di %s fallita\n", pathname);
  }
  return res;
}

int removeFile(const char* pathname) { //rimuove un file dal server
  if(pathname == NULL) {
    errno = EINVAL;
    return -1;
  }
  if(writeCMD(pathname, 'c') == -1) {
    errno = EPERM;
    return -1;
  }
  int res, notused;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &res, sizeof(int)), "read", ""); //legge la risposta del server
  if(res == -1) { //errore nella removeFile
    errno = EACCES;
    fprintf(stderr, "removeFile di %s fallita\n", pathname);
    return -1;
  } else { //tutto ok
    if(verbose)
      fprintf(stdout, "File %s cancellato con successo dal server\n", pathname);
  }
  return 0;
}

int readFile(const char* pathname, void** buf, size_t* size) { //legge un file dal server
  if(pathname == NULL) {
    errno = EINVAL;
    return -1;
  }
  if(writeCMD(pathname, 'r') == -1) {
    errno = EPERM;
    return -1;
  }
  int notused;
  int n;
  SYSCALL_EXIT("readn1", notused, readn(sockfd, &n, sizeof(int)), "read", ""); //legge n, che può essere la size del file in caso di successo o -1 in caso di fallimento
  *size = n;
  if(*size == -1) { //errore: file non esistente
    errno = EPERM;
    fprintf(stderr, "readFile di %s fallita: il file non esiste\n", pathname);
    *buf = NULL;
    *size = 0;
    return -1;
  } else { //tutto ok, legge il file
    ec_null((*buf = malloc(sizeof(char) * n)), "malloc"); //alloco la stringa da scrivere, che sarà del tipo "rfile"
    SYSCALL_EXIT("readn2", notused, readn(sockfd, *buf, n * sizeof(char)), "read", "");
    if(verbose)
      fprintf(stdout, "Ho letto il file %s dal server\n", pathname);
    return 0; //successo
  }
}

int writeBufToDisk(const char* pathname, void* buf, size_t size) { //scrive il buffer nel disco
  if(pathname == NULL || buf == NULL || size < 0) {
    errno = EINVAL;
    return -1;
  }
  int fdfile;
  ec_meno1((fdfile = open(pathname, O_WRONLY | O_APPEND | O_CREAT, 0644)), "open"); //apre il file
  ec_meno1((writen(fdfile, buf, size)), "writen"); //ci scrive sopra il bufefr
  ec_meno1((close(fdfile)), "close"); //chiude il file
  return 0;
}

int readNFiles(int n, const char* dirname) { //legge N file dal server
  if(dirname == NULL) {
    errno = EINVAL;
    return -1;
  }
  char* ntmp; //numero temporaneo da mandare al server
  ec_null((ntmp = malloc(sizeof(char) * 10)), "malloc");
  if(sprintf(ntmp, "%d", n) < 0) { //lo alloca a stringa per poter fare la writeCMD
    perror("snprintf");
    return -1;
  }
  if(writeCMD(ntmp, 'R') == -1) {
    errno = EPERM;
    return -1;
  }
  free(ntmp);
  int notused;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read", ""); //richiede il numero di file che possono essere letti effettivamente dal server
  int lenpathtmp;
  char* buftmp;
  char** arr_buf; //per memorizzare i nomi dei file da leggere per poter poi fare la readFile
  ec_null((arr_buf = malloc(sizeof(char*) * n)), "malloc");
  for(int i = 0; i < n; i++) { //per ogni file da leggere dal server
    SYSCALL_EXIT("readn", notused, readn(sockfd, &lenpathtmp, sizeof(int)), "read", ""); //legge la lunghezza del nome del file da leggere
    ec_null((buftmp = malloc(sizeof(char) * lenpathtmp)), "malloc"); //alloca il buffer su cui scrivere il nome del file
    SYSCALL_EXIT("readn", notused, readn(sockfd, buftmp, lenpathtmp * sizeof(char)), "read", ""); //legge il nome del file da leggere
    arr_buf[i] = buftmp; //lo inserisce nell'array dei nomi dei file
  }
  for(int i = 0; i < n; i++) { //per ogni nome del file deve fare la readFile, e poi scrivere nel disco il risultato
    void* buffile; //buffer letto
    size_t sizebufffile; //size letta
    if(openFile(arr_buf[i], O_OPEN) != -1) { //tutto ok, file aperto
      if(readFile(arr_buf[i], &buffile, &sizebufffile) == -1) { //legge il file
        return -1;
      }
      char path[1024];
      if(snprintf(path, sizeof(path), "%s/%s", dirname, arr_buf[i]) < 0) { perror("snprintf"); return -1; } //path completa su cui scrivere
      if(writeBufToDisk(path, buffile, sizebufffile) == -1) { return -1; } //scrittura nel disco
      if(closeFile(arr_buf[i]) == -1) { return -1; }
    } else { //file non aperto, errore
      errno = EACCES;
      fprintf(stderr, "openFile fallita per il file %s\n", arr_buf[i]);
      return -1;
    }
    free(arr_buf[i]);
  }
  free(arr_buf);
  return n; //ritorna il numero di file letti
}

int appendToFile(const char* pathname, void* buf, size_t size) { //appendToFile nel server
  if(pathname == NULL) {
    errno = EINVAL;
    return -1;
  }
  int notused;
  SYSCALL_EXIT("writen3", notused, writen(sockfd, &size, sizeof(int)), "write", ""); //scrive la size del file
  int cista;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &cista, sizeof(int)), "read", ""); //legge se il file ci sta materialmente nel server (o è più grande della sua capienza)
  if(!cista) { //il file non sta nel server materialmente, neanche se si espellessero tutti i file
    fprintf(stderr, "Errore: il file %s non sta materialmente nel server\n", pathname);
    return -1;
  }
  SYSCALL_EXIT("writen4", notused, writen(sockfd, (char*)buf, size * sizeof(char)), "write", ""); //scrive il buffer del file

  int risposta; //risposta affermativa o negativa di scrittura
  SYSCALL_EXIT("readn", notused, readn(sockfd, &risposta, sizeof(int)), "read", "");
  if(risposta == -1) { //errore
    errno = EACCES;
    fprintf(stderr, "Errore nella scrittura in append del file %s\n", pathname);
  } else { //tutto ok
    if(verbose)
      fprintf(stdout, "File %s scritto correttamente nel server\n", pathname);
  }
  return risposta;
}

int writeFile(const char* pathname) { //scrittura di un file nel server
  if(pathname == NULL) {
    errno = EINVAL;
    return -1;
  }
  if(writeCMD(pathname, 'W') == -1) {
    errno = EPERM;
    return -1;
  }
  int notused, n;

  int risinit;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &risinit, sizeof(int)), "read", ""); //vede se il file esiste ed è stato aperto
  if(risinit == -1) { //errore: il file non esiste o non è stato aperto
    fprintf(stderr, "Errore: il file %s non esiste o non è stato aperto\n", pathname);
    return -1;
  } else { //tutto ok, apre il file da mandare
    struct stat info;
    ec_meno1((stat(pathname, &info)), "stat");
    long length = (long)info.st_size; //lunghezza del file da mandare
    int fdfile; //fd del file
    ec_meno1((fdfile = open(pathname, O_RDONLY)), "open"); //apre il file
    char* bufferFile; //buffer del file da mandare
    ec_null((bufferFile = malloc(sizeof(char) * length)), "malloc");
    ec_meno1((readn(fdfile, bufferFile, length)), "readn"); //legge il file da mandare
    ec_meno1((close(fdfile)), "close");

    if(appendToFile(pathname, bufferFile, (size_t)length) == -1) { //fa la scrittura in append
      free(bufferFile);
      return -1;
    }
    free(bufferFile);
    return 0;
  }
}

int EseguiComandoClientServer(NodoComando *tmp) { //si occupa dell'esecuzione delle API
  if(tmp == NULL) {
    errno = EINVAL;
    return -1;
  }
  if(tmp->cmd == 'c') { //comando di cancellazione di un file dal server
    if(verbose)
      fprintf(stdout, "File da rimuovere: %s\n", tmp->name);
    if(openFile(tmp->name, O_OPEN) != -1) {
      if(removeFile(tmp->name) == -1) return -1;
    } else { //errore
      return -1;
    }
  } else if(tmp->cmd == 'r') { //comando di lettura di un file dal server
    if(verbose)
      fprintf(stdout, "Sto leggendo il file %s\n", tmp->name);
    if(openFile(tmp->name, O_OPEN) != -1) {
      void* buf;
      size_t sizebuff; //col size_t non va
      if(readFile(tmp->name, &buf, &sizebuff) == -1) {
        return -1;
      } else {
        if(savefiledir != NULL && buf != NULL) { //se deve salvare il file in locale
          char path[1024];
          if(snprintf(path, sizeof(path), "%s/%s", savefiledir, tmp->name) < 0) { perror("snprintf"); return -1; }
          if(writeBufToDisk(path, buf, sizebuff) == -1) return -1; //da controllare free nel caso errori
          if(closeFile(tmp->name) == -1) return -1;
        }
        free(buf);
      }
    } else { //errore openFile
      return -1;
    }
  } else if(tmp->cmd == 'R') { //lettura N file dal server
    if(verbose)
      fprintf(stdout, "readNFiles con n = %d\n", tmp->n);
      if(readNFiles(tmp->n, savefiledir) == -1) return -1;
  } else if(tmp->cmd == 'W') { //scrittura di un file dal server
    if(verbose)
      fprintf(stdout, "Scrivo il file %s nel server\n", tmp->name);
    if(openFile(tmp->name, O_CREATE) != -1) { //flag: apri e crea
      if(writeFile(tmp->name) == -1) { return -1; }
      if(closeFile(tmp->name) == -1) { return -1; }
    } else if(openFile(tmp->name, O_OPEN) != -1) { //flag: apri (magari il file esiste già, nel caso fa la scrittura in append)
      if(writeFile(tmp->name) == -1) { return -1; }
      if(closeFile(tmp->name) == -1) return -1;
    } else { //errore openFile
      return -1;
    }
  } else { //errore: parametro non riconosciuto
    if(verbose)
      fprintf(stderr, "Errore: parametro %c non riconosciuto\n", tmp->cmd);
    return -1;
  }
  return 0;
}

void printQueueCMD(Queue *q) { //stampa la coda dei comandi
  Node* tmp = q->head;
  NodoComando *no = NULL;
  while(tmp != NULL) {
    no = tmp->data;
    fprintf(stdout, "Nome del file è %s, len %ld\n", no->name, strlen(no->name));
    tmp = tmp->next;
  }
}

int visitaRicorsiva(char* name, int *n, Queue **q) { //comando 'w', fa una visita ricorsiva per ottenere i file da mandare al server
  if(name == NULL)
    return -1;
  char buftmp[1024];
  if (getcwd(buftmp, 1024) == NULL) { perror("getcwd");  return -1; } //ottiene la dir corrente per poi tornarci a fine iterazione
  if (chdir(name) == -1) { perror("chdir2"); fprintf(stderr, "errno %d visitando %s\n", errno, name);  return -1; }   //mi sposto nella nuova dir
  DIR *dir;
  struct dirent *entry;

  if (!(dir = opendir(name)))
      return -1;

  while ((entry = readdir(dir)) != NULL && (*n != 0)) { //legge le entry nella directory
    char path[1024];
    if (entry->d_type == DT_DIR) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || entry->d_name[0] == '.')
        continue;
      if(snprintf(path, sizeof(path), "%s/%s", name, entry->d_name) < 0) { perror("snprintf"); return -1; } //ottiene la path assoluta

        if(visitaRicorsiva(path, n, q) == -1) return -1; //fa la visita ricorsiva nella nuova dir
      } else if (entry->d_type == DT_REG) { //ogni file regolare che vede, deve decrementare n
        if(*n > 0 || *n == -1) { //nuovo file da scrivere
          char *buffer;
          ec_null((buffer = malloc(sizeof(char) * 1024)), "malloc");
          ec_null((realpath(entry->d_name, buffer)), "realpath");
          NodoComando *new; //alloca un nuovo comando
          ec_null((new = malloc(sizeof(NodoComando))), "malloc");
          new->cmd = 'W';
          ec_null((new->name = malloc(sizeof(char) * (strlen(buffer) + 1))), "malloc");
          new->name[strlen(buffer)] = '\0';
          new->n = 0;
          strncpy(new->name, buffer, strlen(buffer));
          if(pushTesta(q, new) == -1) return -1; //inserisce il nuovo comando in testa alla coda
          free(buffer);
        }
        if(*n > 0)
          (*n)--;
      }
  }
  ec_meno1((closedir(dir)), "closedir"); //chiude la directory
  ec_meno1((chdir(buftmp)), "chdir"); //ritorna nella directory vecchia
  return 0;
}

int main(int argc, char *argv[]) {

  //maschera il segnale SIGPIPE (motivo descritto nella relazione)
  struct sigaction sapipe;
  memset(&sapipe, 0, sizeof(struct sigaction));
  sapipe.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sapipe, NULL);


  Queue *q = parser(argc, argv); //ottiene coda delle operazioni
  struct timespec abstime;

  add_to_current_time(10, 0, &abstime);
  //primo parametro: tempo limite (in secondi)
  //secondo parametro: intervallo di tempo tra due connessioni (in millisecondi)

  ec_meno1((openConnection(socknameconfig, 1000, abstime)), "openConnection"); //apre la connessione con il server

  //intervallo di tempo tra due comandi successivi
  abstime.tv_sec = timems / 1000;
  abstime.tv_nsec = (timems % 1000) * 1000000;

  while(q->len > 0) { //finchè ci sono richieste che il parser ha visto
    NodoComando *tmp = pop(&q); //fa la pop dalla FIFO dei comandi
    nanosleep(&abstime, NULL); //aspetta
    if(verbose)
      fprintf(stdout, "Sto processando il comando '%c' con parametro %s\n", tmp->cmd, tmp->name);
    if(tmp->cmd == 'w') { //se il comando è 'w' non fa una richiesta al server, ma visita ricorsivamente e fa una richiesta a parte per ogni file
      if(tmp->n == 0)
        tmp->n = -1;

      if(strcmp(tmp->name, ".") == 0) { //trasformo le dir . in directory path complete prima di chiamare la visitaRicorsiva
        free(tmp->name);
        ec_null((tmp->name = malloc(sizeof(char) * 1024)), "malloc");
        ec_null((getcwd(tmp->name, 1024)), "getcwd");
      }

      if(visitaRicorsiva(tmp->name, &(tmp->n), &q) == -1) return -1;
      free(tmp->name);
    } else { //comando normale, lo esegue
      if(EseguiComandoClientServer(tmp) == -1) fprintf(stderr, "Errore nel comando %c con parametro %s\n", tmp->cmd, tmp->name);

      //pulizia tmp
      free(tmp->name);
      free(tmp);
    }

  }

  //ha finito le operazioni da eseguire, esce
  int notused;
  int n = -1;
  ec_meno1((closeConnection(socknameconfig)), "closeConnection"); //chiude la connessione con il server
  free(socknameconfig);
  free(savefiledir);
  free(q);
  return 0;
}
