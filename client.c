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
#include "util.h"
#include "parser.h"

#define SOCKNAME "ProvaSock.sk"
#define MAXBUFFER 1000
long sockfd;

static int add_to_current_time(long sec, long nsec, struct timespec* res){
    // TODO: maybe check its result
  clock_gettime(CLOCK_REALTIME, res);
  res->tv_sec += sec;
  res->tv_nsec += nsec;

  return 0;
}


int set_timespec_from_msec(long msec, struct timespec* req) {
  if(msec < 0 || req == NULL){
    errno = EINVAL;
    return -1;
  }

  req->tv_sec = msec / 1000;
  msec = msec % 1000;
  req->tv_nsec = msec * 1000;

  return 0;
}



int openConnection(const char* sockname, int msec, const struct timespec abstime) {


    if(sockname == NULL || msec < 0) { //argomenti non validi
        errno = EINVAL;
        return -1;
    }




    struct sockaddr_un serv_addr;
    //int sockfd;
    SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, socknameconfig, strlen(socknameconfig) + 1);

    /*int notused;
    SYSCALL_EXIT("connect", notused, connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "connect", "");*/
    // setting waiting time
    struct timespec wait_time;
    // no need to check because msec > 0 and &wait_time != NULL
    set_timespec_from_msec(msec, &wait_time);

    // setting current time
    struct timespec curr_time;
    clock_gettime(CLOCK_REALTIME, &curr_time);

    //fprintf(stderr, "fin qui");
    //sleep(3);
    // trying to connect
    int err = -1;
    fprintf(stderr, "currtime %ld abstime %ld\n", curr_time.tv_sec, abstime.tv_sec);
    while( (err = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) == -1
            && curr_time.tv_sec < abstime.tv_sec ) {
        //debug("connect didn't succeed, trying again...\n");
        //fprintf(stderr, "err4 %d\n", errno);

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
        //debug("Could not connect to server. :(\n");

        sockfd = -1;
        errno = ETIMEDOUT;
        return -1;
    }

    //debug("Connected! :D\n");

    //socket_path = sockname;
    if(verbose)
      fprintf(stdout, "Mi sono connesso al server!\n");
    return 0;
}

int closeConnection(const char* sockname){
  if(sockname == NULL){
    errno = EINVAL;
    return -1;
  }
    // wrong socket name
  if(strcmp(sockname, socknameconfig) != 0){
        // this socket is not connected
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

int writeCMD(const char* pathname, char cmd) { //manca gestione errore
  //fprintf(stderr, )
  if(pathname == NULL){
    errno = EINVAL;
    return -1;
  }
  int notused;
  char *buffer = NULL;
  char* towrite;
  ec_null((towrite = malloc(sizeof(char) * (strlen(pathname) + 2))), "malloc"); //alloco la stringa da scrivere, che sarà del tipo "rfile"
  towrite[0] = cmd;
  for(int i = 1; i <= strlen(pathname); i++)
    towrite[i] = pathname[i - 1];
  towrite[strlen(pathname) + 1] = '\0';
  //fprintf(stderr, "sto scrivendo nel socket %s, nome file originale %s\n", towrite, pathname);
  int n = strlen(towrite) + 1; //terminatore

  SYSCALL_EXIT("writen", notused, writen(sockfd, &n, sizeof(int)), "write", "");
  SYSCALL_EXIT("writen", notused, writen(sockfd, towrite, n * sizeof(char)), "write", "");
  return 0;

}

int closeFile(const char* pathname) {
  if(pathname == NULL) {
    errno = EINVAL;
    return -1;
  }

  if(writeCMD(pathname, 'z') == -1) {
    errno = EPERM;
    return -1;
  }
  int res, notused;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &res, sizeof(int)), "read", "");
  if(res == -1) {
    errno = EPERM;
    if(verbose)
      fprintf(stdout, "closeFile di %s fallita\n", pathname);
  } else { //res = 0
    if(verbose)
      fprintf(stdout, "Ho fatto la closeFile di %s\n", pathname);
  }
  return res;
}

int openFile(const char* pathname, int flags) {
  if((flags != 0 && flags != 1) || pathname == NULL) { // i flag passate non sono valide (0 -> open | 1 -> open & create)
    errno = EINVAL;
    return -1;
  }
  if(writeCMD(pathname, 'o') == -1) {
    errno = EPERM;
    return -1;
  }
  int res, notused;
  SYSCALL_EXIT("writen", notused, writen(sockfd, &flags, sizeof(int)), "write", "");
  SYSCALL_EXIT("readn", notused, readn(sockfd, &res, sizeof(int)), "read", "");
  if(res == 0) {
    if(verbose)
      fprintf(stdout, "Ho fatto la openFile di %s\n", pathname);
  } else { //res = -1, errore
    errno = EACCES;
    if(verbose)
      fprintf(stdout, "openFile di %s fallita\n", pathname);
  }
  return res;
}

int removeFile(const char* pathname) {
  if(pathname == NULL) {
    errno = EINVAL;
    return -1;
  }
  if(writeCMD(pathname, 'c') == -1) {
    errno = EPERM;
    return -1;
  }
  int res, notused;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &res, sizeof(int)), "read", "");
  if(res == -1) { //errore nella removeFile
    errno = EACCES;
    if(verbose)
      fprintf(stdout, "removeFile di %s fallita\n", pathname);
    return -1;
  } else {
    if(verbose)
      fprintf(stdout, "File %s cancellato con successo dal server\n", pathname);
  }
  return 0;
}

int readFile(const char* pathname, void** buf, int* size) {
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
  SYSCALL_EXIT("readn1", notused, readn(sockfd, &n, sizeof(int)), "read", "");
  *size = n;
  if(*size == -1) { //file non esistente
    errno = EPERM;
    if(verbose)
      fprintf(stdout, "readFile di %s fallita: il file non esiste\n", pathname);
    *buf = NULL;
    *size = 0;
    return -1;
  } else {
    ec_null((*buf = malloc(sizeof(char) * n)), "malloc"); //alloco la stringa da scrivere, che sarà del tipo "rfile"
    SYSCALL_EXIT("readn2", notused, readn(sockfd, *buf, n * sizeof(char)), "read", "");
    if(verbose)
      fprintf(stdout, "Ho letto il file %s dal server\n", pathname);
    //fprintf(stderr, "CIAOOOOOO\n");
    //fprintf(stderr, "ho letto il file %s con contenuto\n %s\n", pathname, (char*)(*buf));
    return 0; //successo
  }
}

//int writeBufToDisk(const char* dirname, char* filename, char* buf, int len) {
int writeBufToDisk(const char* pathname, void* buf, int size) {
  //char path[1024];
  //snprintf(path, sizeof(path), "%s/%s", dirname, filename);
  if(pathname == NULL || buf == NULL || size < 0) {
    errno = EINVAL;
    return -1;
  }
  FILE *f;
  ec_null((f = fopen(pathname, "w")), "fopen");
  if(fwrite(buf, 1, size, f) < size) {
    perror("fwrite");
    return -1;
  }
  neq_zero((fclose(f)), "fclose");
  return 0;
}

int readNFiles(int n, const char* dirname) {
  if(dirname == NULL) {
    errno = EINVAL;
    return -1;
  }
  char* ntmp;
  ec_null((ntmp = malloc(sizeof(char) * 10)), "malloc");
  if(sprintf(ntmp, "%d", n) < 0) {
    perror("snprintf");
    return -1;
  }
  if(writeCMD(ntmp, 'R') == -1) {
    errno = EPERM;
    return -1;
  }

  int notused;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read", "");
  int lenpathtmp;
  char* buftmp;
  char** arr_buf;
  ec_null((arr_buf = malloc(sizeof(char*) * n)), "malloc");//abbiamo messo l'array perchè sennò fa casino con le read della readFile
  for(int i = 0; i < n; i++) { //per ogni file da leggere dal server
    SYSCALL_EXIT("readn", notused, readn(sockfd, &lenpathtmp, sizeof(int)), "read", "");
    ec_null((buftmp = malloc(sizeof(char) * lenpathtmp)), "malloc");
    SYSCALL_EXIT("readn", notused, readn(sockfd, buftmp, lenpathtmp * sizeof(char)), "read", "");
    //fprintf(stderr, "leggo il file %s dal server\n", buftmp);
    arr_buf[i] = buftmp;
    //void* buffile;
    //int sizebufffile;
    //readFile(buftmp, &buffile, &sizebufffile);
  }
  //fprintf(stderr, "\n\n\n STO STAMPANDO L'ARRAY \n\n\n");
  for(int i = 0; i < n; i++) {
    //fprintf(stderr, "elemento %d: %s\n", i, arr_buf[i]);
    void* buffile;
    int sizebufffile;
    if(openFile(arr_buf[i], 0) != -1) {
      if(readFile(arr_buf[i], &buffile, &sizebufffile) == -1) {
        return -1;
      }
      char path[1024];
      //fprintf(stderr, "fin qui ci siamo\n");
      if(snprintf(path, sizeof(path), "%s/%s", dirname, arr_buf[i]) < 0) { perror("snprintf"); return -1; }
      //fprintf(stderr, "fin qui ci siamo, dirname %s, path %s\n", dirname, path);

      if(writeBufToDisk(path, buffile, sizebufffile) == -1) { return -1; }
      if(closeFile(arr_buf[i]) == -1) { return -1; }
    } else {
      errno = EACCES;
      if(verbose)
        fprintf(stdout, "openFile fallita per il file %s\n", arr_buf[i]);
      return -1;
    }
  }
  //SYSCALL_EXIT("writen", notused, writen(sockfd, &n, sizeof(int)), "write", "");

}

int appendToFile(const char* pathname, void* buf, int size) {
  if(pathname == NULL) {
    errno = EINVAL;
    return -1;
  }
  int notused;
  //fprintf(stderr, "length file %s: %d\n", pathname, size);
  SYSCALL_EXIT("writen3", notused, writen(sockfd, &size, sizeof(int)), "write", "");
  int cista;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &cista, sizeof(int)), "read", "");
  if(!cista) { //il file non sta nel server materialmente, neanche se si espellessero tutti i file
    if(verbose)
      fprintf(stdout, "Il file %s non sta materialmente nel server\n", pathname);
      //vanno fatte delle FREE
    return -1;
  }
  SYSCALL_EXIT("writen4", notused, writen(sockfd, (char*)buf, size * sizeof(char)), "write", "");

  int risposta;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &risposta, sizeof(int)), "read", "");
  if(risposta == -1) {
    errno = EACCES;
    if(verbose)
      fprintf(stderr, "Errore nella scrittura in append del file %s\n", pathname);
  } else {
    if(verbose)
      fprintf(stdout, "File %s scritto correttamente nel server\n", pathname);
  }
  //printf("result: %d\n", risposta);
  return risposta;
}

int writeFile(const char* pathname) {
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
  SYSCALL_EXIT("readn", notused, readn(sockfd, &risinit, sizeof(int)), "read", "");
  if(risinit == -1) {
    if(verbose)
      fprintf(stderr, "Errore: il file %s non esiste o non è stato aperto\n", pathname);
    return -1;
  } else {
    char *buffer = NULL;
    n = strlen(pathname) + 2;
    ec_null((buffer = realloc(buffer, n * sizeof(char))), "realloc");

    //usare al posto la open, readn, writen, close
    FILE *f;
    ec_null((f = fopen(pathname, "rb")), "fopen");
    long length;
    char* bufferFile;
    if (f) {
      ec_meno1((fseek(f, 0, SEEK_END)), "fseek");
      //fseek (f, 0, SEEK_END);
      //length = ftell(f);
      ec_meno1((length = ftell(f)), "ftell");
      ec_meno1((fseek(f, 0, SEEK_SET)), "fseek");
      //fseek (f, 0, SEEK_SET);
      ec_null((bufferFile = malloc(length)), "malloc");
      if (bufferFile) {
        fread(bufferFile, 1, length, f);
      }
      neq_zero((fclose(f)), "fclose");
    }

    if(appendToFile(pathname, bufferFile, length) == -1)
      return -1;
    return 0;
  }
}

int EseguiComandoClientServer(NodoComando *tmp) {
  if(tmp == NULL) {
    errno = EINVAL;
    return -1;
  }
  if(tmp->cmd == 'c') {
    if(verbose)
      fprintf(stdout, "File da rimuovere: %s\n", tmp->name);
    if(openFile(tmp->name, 0) != -1) {
      if(removeFile(tmp->name) == -1) return -1;
    } else { //errore
      return -1;
    }
  } else if(tmp->cmd == 'r') {
    if(verbose)
      fprintf(stdout, "Sto leggendo il file %s\n", tmp->name);
    if(openFile(tmp->name, 0) != -1) {
      void* buf;
      int sizebuff; //col size_t non va
      if(readFile(tmp->name, &buf, &sizebuff) == -1) {
        return -1;
      } else {
        if(savefiledir != NULL && buf != NULL) {
          char path[1024];
          if(snprintf(path, sizeof(path), "%s/%s", savefiledir, tmp->name) < 0) { perror("snprintf"); return -1; }
          //fprintf(stderr, "sto andando a scrivere il file %s in %s\n", tmp->name, path);
          if(writeBufToDisk(path, buf, sizebuff) == -1) return -1;
          if(closeFile(tmp->name) == -1) return -1;
        }
      }
    } else { //errore openFile
      return -1;
    }
  } else if(tmp->cmd == 'R') {
    if(verbose)
      fprintf(stdout, "readNFiles con n = %d\n", tmp->n);
      if(readNFiles(tmp->n, savefiledir) == -1) return -1;
  } else if(tmp->cmd == 'W') {
    if(verbose)
      fprintf(stdout, "Scrivo il file %s nel server\n", tmp->name);
    if(openFile(tmp->name, 1) != -1) { //flag: apri e crea
      //fprintf(stderr, "HO FATTO LA OPEN del file %s\n", tmp->name);
      if(writeFile(tmp->name) == -1) { return -1; }
      if(closeFile(tmp->name) == -1) return -1;
    } else if(openFile(tmp->name, 0) != -1) { //flag: apri (magari il file esiste già)
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
  //da qui solo se il comando è W
  return 0;
}

void printQueuee(Queue *q) {
  Node* tmp = q->head;
  NodoComando *no = NULL;
  while(tmp != NULL) {
    no = tmp->data;
    fprintf(stdout, "sto nome del file è %s, len %ld\n", no->name, strlen(no->name));
    tmp = tmp->next;
  }
}

void visitaRicorsiva(char* name, int *n, Queue **q) {
  //fprintf(stderr, "sto visitando %s\n", name);
  if(name == NULL)
    return;
  char buftmp[1024];
  if (getcwd(buftmp, 1024)==NULL) { perror("getcwd");  exit(EXIT_FAILURE); }
  //mi sposto nella nuova dir
  if (chdir(name) == -1) { perror("chdir2"); fprintf(stderr, "errno %d visitando %s\n", errno, name);  exit(EXIT_FAILURE); }
  DIR *dir;
  struct dirent *entry;

  if (!(dir = opendir(name)))
      return;

  while ((entry = readdir(dir)) != NULL && (*n != 0)) {
    char path[1024];
    if (entry->d_type == DT_DIR) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || entry->d_name[0] == '.')
        continue;
        snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
        //printf("%*s[%s]\n", 0, "", entry->d_name);
        //fprintf(stderr, "sto chiamando la funzione su %s\n", entry->d_name);

        visitaRicorsiva(path, n, q);
      } else if (entry->d_type == DT_REG) { //ogni file regolare che vede, deve decrementare n
        if(*n > 0 || *n == -1) {
          //snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
          char *buffer = malloc(sizeof(char) * 1024);
          realpath(entry->d_name, buffer);
          printf("%*s- %s, realpath %s, strlen realpath %lu\n", 0, "", entry->d_name, buffer, strlen(buffer));
          //buffer[strlen(buffer)] = '\0';
          NodoComando *new = malloc(sizeof(NodoComando));
          new->cmd = 'W';
          new->name = malloc(sizeof(char) * (strlen(buffer) + 1));
          new->name[strlen(buffer)] = '\0';
          new->n = 0;
          strncpy(new->name, buffer, strlen(buffer));
          push(q, new);
          fprintf(stderr, "HO APPENA SCRITTO %s, strlen %ld\n", new->name, strlen(new->name));
          //printQueuee(*q);
        }
        if(*n > 0)
          (*n)--;
      }
  }
  closedir(dir);
  if (chdir(buftmp) == -1) { perror("chdir");  exit(EXIT_FAILURE); }

}

int main(int argc, char *argv[]) {
  Queue *q = parser(argc, argv); //coda delle operazioni
  struct timespec abstime;

  add_to_current_time(10, 0, &abstime);
  //primo parametro: tempo limite (in secondi)
  //secondo parametro: intervallo di tempo tra due connessioni (in millisecondi)

  /*struct sockaddr_un serv_addr;
  int sockfd;
  SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
  memset(&serv_addr, '0', sizeof(serv_addr));

  serv_addr.sun_family = AF_UNIX;
  strncpy(serv_addr.sun_path,SOCKNAME, strlen(SOCKNAME)+1);

  int notused;
  SYSCALL_EXIT("connect", notused, connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "connect", "");*/
  int x = openConnection(socknameconfig, 1000, abstime); //da vedere se da errore
  abstime.tv_sec = timems / 1000;
  abstime.tv_nsec = (timems % 1000) * 1000000;
  fprintf(stderr, "sockfd: %ld, risultato openconnection %d\n", sockfd, x);


  while(q->len > 0) { //finchè ci sono richieste che il parser ha visto
    NodoComando *tmp = pop(&q);
    nanosleep(&abstime, NULL);
    fprintf(stderr, "FAIL NEIM %s\n", tmp->name);
    if(tmp->cmd == 'w') { //non fa una richiesta al server, ma visita ricorsivamente e fa una richiesta a parte per ogni file
      if(tmp->n == 0)
        tmp->n = -1;

      //trasformo le dir . in directory path complete prima di chiamare la visitaRicorsiva
      //ho fatto questo perchè almeno la visitaRicorsiva non fa confusione con la directory '.'
      if(strcmp(tmp->name, ".") == 0) {
        free(tmp->name);
        tmp->name = malloc(sizeof(char) * 1024);
        if (getcwd(tmp->name, 1024) == NULL) { perror("getcwd");  exit(EXIT_FAILURE); }
      }

      visitaRicorsiva(tmp->name, &(tmp->n), &q);
    } else {
      EseguiComandoClientServer(tmp);
    }

  }
  //openFile("client.cs", 0);
  closeConnection(socknameconfig);
  return 0;
}
