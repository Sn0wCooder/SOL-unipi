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
    strncpy(serv_addr.sun_path,SOCKNAME, strlen(SOCKNAME)+1);

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
            && curr_time.tv_sec < abstime.tv_sec ){
        //debug("connect didn't succeed, trying again...\n");
        fprintf(stderr, "err4 %d\n", errno);

        if( nanosleep(&wait_time, NULL) == -1){
            sockfd = -1;
            return -1;
        }
        if( clock_gettime(CLOCK_REALTIME, &curr_time) == -1){
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
    fprintf(stderr, "mi sono connesso al server!\n");
    return 0;
}

int writeCMD(const char* pathname, char cmd) { //manca gestione errore
  //fprintf(stderr, )
  int notused;
  char *buffer = NULL;
  char* towrite = malloc(sizeof(char) * (strlen(pathname) + 2)); //alloco la stringa da scrivere, che sarà del tipo "rfile"
  towrite[0] = cmd;
  for(int i = 1; i <= strlen(pathname); i++)
    towrite[i] = pathname[i - 1];
  towrite[strlen(pathname) + 1] = '\0';
  fprintf(stderr, "sto scrivendo nel socket %s, nome file originale %s\n", towrite, pathname);
  int n = strlen(towrite) + 1; //terminatore

  SYSCALL_EXIT("writen", notused, writen(sockfd, &n, sizeof(int)), "write", "");
  SYSCALL_EXIT("writen", notused, writen(sockfd, towrite, n * sizeof(char)), "write", "");

}

int removeFile(const char* pathname) {
  writeCMD(pathname, 'c');
  int res, notused;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &res, sizeof(int)), "read", "");
  if(res != 1) { perror("sa_success"); return -1; }
  fprintf(stderr, "File %s cancellato con successo dal server\n", pathname);
  return 0;
}

int readFile(const char* pathname, void** buf, int* size) {
  fprintf(stderr, "pathname a questo punto %s\n", pathname);
  writeCMD(pathname, 'r');
  int notused;
  int n;
  SYSCALL_EXIT("readn1", notused, readn(sockfd, &n, sizeof(int)), "read", "");
  *size = n;
  //fprintf(stderr, "la size del file che sto provando ad allocare è %d\n", n);
  if(*size == -1) {
    fprintf(stderr, "file %s non esiste\n", pathname);
    *buf = NULL;
    *size = 0;
    return -1; //errore
  } else {
    *buf = malloc(sizeof(char) * n);
    SYSCALL_EXIT("readn2", notused, readn(sockfd, *buf, n * sizeof(char)), "read", "");
    fprintf(stderr, "ho letto il file %s dal server\n", pathname);
    //fprintf(stderr, "CIAOOOOOO\n");
    //fprintf(stderr, "ho letto il file %s con contenuto\n %s\n", pathname, (char*)(*buf));
    return 0; //successo
  }
}

int writeBufToDisk(const char* dirname, char* filename, char* buf, int len) {
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s", dirname, filename);
  FILE *f = fopen (path, "w"); //gestire errori
  fwrite(buf, 1, len, f);
  fclose(f);
  return 1;
}

int readNFiles(int n, const char* dirname) {
  char* ntmp = malloc(sizeof(char) * 10);
  sprintf(ntmp, "%d", n);
  writeCMD(ntmp, 'R');

  int notused;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read", "");
  int lenpathtmp;
  char* buftmp;
  char** arr_buf = malloc(sizeof(char*) * n); //abbiamo messo l'array perchè sennò fa casino con le read della readFile
  for(int i = 0; i < n; i++) { //per ogni file da leggere dal server
    SYSCALL_EXIT("readn", notused, readn(sockfd, &lenpathtmp, sizeof(int)), "read", "");
    buftmp = malloc(sizeof(char) * lenpathtmp);
    SYSCALL_EXIT("readn", notused, readn(sockfd, buftmp, lenpathtmp * sizeof(char)), "read", "");
    //fprintf(stderr, "leggo il file %s dal server\n", buftmp);
    arr_buf[i] = buftmp;
    //void* buffile;
    //int sizebufffile;
    //readFile(buftmp, &buffile, &sizebufffile);
  }
  fprintf(stderr, "\n\n\n STO STAMPANDO L'ARRAY \n\n\n");
  for(int i = 0; i < n; i++) {
    fprintf(stderr, "elemento %d: %s\n", i, arr_buf[i]);
    void* buffile;
    int sizebufffile;
    readFile(arr_buf[i], &buffile, &sizebufffile);
    writeBufToDisk(dirname, arr_buf[i], buffile, sizebufffile);
  }
  //SYSCALL_EXIT("writen", notused, writen(sockfd, &n, sizeof(int)), "write", "");

}

int writeFile(const char* pathname) {
  int notused, n;
  writeCMD(pathname, 'W');
  char *buffer = NULL;
  n = strlen(pathname) + 2;
  buffer = realloc(buffer, n*sizeof(char));
  if (!buffer) { perror("realloc"); fprintf(stderr, "Memoria esaurita....\n"); }


  SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read", "");
    //fprintf(stderr, "e fin qui\n");
  SYSCALL_EXIT("readn", notused, readn(sockfd, buffer, n * sizeof(char)), "read", "");
  buffer[n] = '\0';
  fprintf(stderr, "buffer %s\n", buffer);
  if(strcmp(buffer, "file ok") == 0) {
    fprintf(stderr, "file ok\n");

    FILE * f = fopen (pathname, "rb");

      /*struct stat info;
      if (stat(tmp->name, &info) == -1){ perror("stat"); exit(EXIT_FAILURE); }
      long size = (long)info.st_size;*/


    long length;
    char* bufferFile;
    if (f) {
      fseek (f, 0, SEEK_END);
      length = ftell (f);
      fseek (f, 0, SEEK_SET);
      bufferFile = malloc (length);
      if (bufferFile)
      {
        fread (bufferFile, 1, length, f);
      }
      fclose (f);
    }

      //fprintf(stderr, "e fin qui\n");
      //int x = 3;
      //int y = 4;

    fprintf(stderr, "length file %s: %ld\n", pathname, length);
    SYSCALL_EXIT("writen3", notused, writen(sockfd, &length, sizeof(int)), "write", "");
    SYSCALL_EXIT("writen4", notused, writen(sockfd, bufferFile, length * sizeof(char)), "write", "");

    buffer = realloc(buffer, n*sizeof(char));
    if (!buffer) { perror("realloc"); fprintf(stderr, "Memoria esaurita....\n"); }


    SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read", "");
      //fprintf(stderr, "e fin qui\n");
    SYSCALL_EXIT("readn", notused, readn(sockfd, buffer, n * sizeof(char)), "read", "");
    buffer[n] = '\0';
    printf("result: %s\n", buffer);
  } else {
    fprintf(stderr, "file già esistente nel server\n");
  }
}

int EseguiComandoClientServer(NodoComando *tmp) {
  if(tmp->cmd == 'c') {
    fprintf(stderr, "file da rimuovere %s\n", tmp->name);
    removeFile(tmp->name);
    return 0;
  } else if(tmp->cmd == 'r') {
    fprintf(stderr, "sto eseguendo sta parte\n");
    void* buf;
    int sizebuff; //col size_t non va
    readFile(tmp->name, &buf, &sizebuff); //manca gestione errore
    if(savefiledir != NULL && buf != NULL)
      writeBufToDisk(savefiledir, tmp->name, buf, sizebuff);
    return 0;
  } else if(tmp->cmd == 'R') {
    fprintf(stderr, "comando readNFiles con n = %d\n", tmp->n);
    readNFiles(tmp->n, savefiledir);
    return 0;
  } else if(tmp->cmd == 'W') {
    fprintf(stderr, "comando W con parametro %s\n", tmp->name);
    writeFile(tmp->name);
  }
  //da qui solo se il comando è W

}

int openFile(const char* pathname, int flags) {
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

  add_to_current_time(2, 0, &abstime);
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
  int x = openConnection(SOCKNAME, 0, abstime); //da vedere se da errore
  fprintf(stderr, "sockfd: %ld, risultato openconnection %d\n", sockfd, x);


  while(q->len > 0) { //finchè ci sono richieste che il parser ha visto
    NodoComando *tmp = pop(&q);
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
  close(sockfd);
  return 0;
}
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
    strncpy(serv_addr.sun_path,SOCKNAME, strlen(SOCKNAME)+1);

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
            && curr_time.tv_sec < abstime.tv_sec ){
        //debug("connect didn't succeed, trying again...\n");
        fprintf(stderr, "err4 %d\n", errno);

        if( nanosleep(&wait_time, NULL) == -1){
            sockfd = -1;
            return -1;
        }
        if( clock_gettime(CLOCK_REALTIME, &curr_time) == -1){
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
    fprintf(stderr, "mi sono connesso al server!\n");
    return 0;
}

int writeCMD(const char* pathname, char cmd) { //manca gestione errore
  //fprintf(stderr, )
  int notused;
  char *buffer = NULL;
  char* towrite = malloc(sizeof(char) * (strlen(pathname) + 2)); //alloco la stringa da scrivere, che sarà del tipo "rfile"
  towrite[0] = cmd;
  for(int i = 1; i <= strlen(pathname); i++)
    towrite[i] = pathname[i - 1];
  towrite[strlen(pathname) + 1] = '\0';
  fprintf(stderr, "sto scrivendo nel socket %s, nome file originale %s\n", towrite, pathname);
  int n = strlen(towrite) + 1; //terminatore

  SYSCALL_EXIT("writen", notused, writen(sockfd, &n, sizeof(int)), "write", "");
  SYSCALL_EXIT("writen", notused, writen(sockfd, towrite, n * sizeof(char)), "write", "");

}

int removeFile(const char* pathname) {
  writeCMD(pathname, 'c');
  int res, notused;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &res, sizeof(int)), "read", "");
  if(res != 1) { perror("sa_success"); return -1; }
  fprintf(stderr, "File %s cancellato con successo dal server\n", pathname);
  return 0;
}

int readFile(const char* pathname, void** buf, int* size) {
  fprintf(stderr, "pathname a questo punto %s\n", pathname);
  writeCMD(pathname, 'r');
  int notused;
  int n;
  SYSCALL_EXIT("readn1", notused, readn(sockfd, &n, sizeof(int)), "read", "");
  *size = n;
  //fprintf(stderr, "la size del file che sto provando ad allocare è %d\n", n);
  if(*size == -1) {
    fprintf(stderr, "file %s non esiste\n", pathname);
    *buf = NULL;
    *size = 0;
    return -1; //errore
  } else {
    *buf = malloc(sizeof(char) * n);
    SYSCALL_EXIT("readn2", notused, readn(sockfd, *buf, n * sizeof(char)), "read", "");
    fprintf(stderr, "ho letto il file %s dal server\n", pathname);
    //fprintf(stderr, "CIAOOOOOO\n");
    //fprintf(stderr, "ho letto il file %s con contenuto\n %s\n", pathname, (char*)(*buf));
    return 0; //successo
  }
}

int writeBufToDisk(const char* dirname, char* filename, char* buf, int len) {
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s", dirname, filename);
  FILE *f = fopen (path, "w"); //gestire errori
  fwrite(buf, 1, len, f);
  fclose(f);
  return 1;
}

int readNFiles(int n, const char* dirname) {
  char* ntmp = malloc(sizeof(char) * 10);
  sprintf(ntmp, "%d", n);
  writeCMD(ntmp, 'R');

  int notused;
  SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read", "");
  int lenpathtmp;
  char* buftmp;
  char** arr_buf = malloc(sizeof(char*) * n); //abbiamo messo l'array perchè sennò fa casino con le read della readFile
  for(int i = 0; i < n; i++) { //per ogni file da leggere dal server
    SYSCALL_EXIT("readn", notused, readn(sockfd, &lenpathtmp, sizeof(int)), "read", "");
    buftmp = malloc(sizeof(char) * lenpathtmp);
    SYSCALL_EXIT("readn", notused, readn(sockfd, buftmp, lenpathtmp * sizeof(char)), "read", "");
    //fprintf(stderr, "leggo il file %s dal server\n", buftmp);
    arr_buf[i] = buftmp;
    //void* buffile;
    //int sizebufffile;
    //readFile(buftmp, &buffile, &sizebufffile);
  }
  fprintf(stderr, "\n\n\n STO STAMPANDO L'ARRAY \n\n\n");
  for(int i = 0; i < n; i++) {
    fprintf(stderr, "elemento %d: %s\n", i, arr_buf[i]);
    void* buffile;
    int sizebufffile;
    readFile(arr_buf[i], &buffile, &sizebufffile);
    writeBufToDisk(dirname, arr_buf[i], buffile, sizebufffile);
  }
  //SYSCALL_EXIT("writen", notused, writen(sockfd, &n, sizeof(int)), "write", "");

}

int writeFile(const char* pathname) {
  int notused, n;
  writeCMD(pathname, 'W');
  char *buffer = NULL;
  n = strlen(pathname) + 2;
  buffer = realloc(buffer, n*sizeof(char));
  if (!buffer) { perror("realloc"); fprintf(stderr, "Memoria esaurita....\n"); }


  SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read", "");
    //fprintf(stderr, "e fin qui\n");
  SYSCALL_EXIT("readn", notused, readn(sockfd, buffer, n * sizeof(char)), "read", "");
  buffer[n] = '\0';
  fprintf(stderr, "buffer %s\n", buffer);
  if(strcmp(buffer, "file ok") == 0) {
    fprintf(stderr, "file ok\n");

    FILE * f = fopen (pathname, "rb");

      /*struct stat info;
      if (stat(tmp->name, &info) == -1){ perror("stat"); exit(EXIT_FAILURE); }
      long size = (long)info.st_size;*/


    long length;
    char* bufferFile;
    if (f) {
      fseek (f, 0, SEEK_END);
      length = ftell (f);
      fseek (f, 0, SEEK_SET);
      bufferFile = malloc (length);
      if (bufferFile)
      {
        fread (bufferFile, 1, length, f);
      }
      fclose (f);
    }

      //fprintf(stderr, "e fin qui\n");
      //int x = 3;
      //int y = 4;

    fprintf(stderr, "length file %s: %ld\n", pathname, length);
    SYSCALL_EXIT("writen3", notused, writen(sockfd, &length, sizeof(int)), "write", "");
    SYSCALL_EXIT("writen4", notused, writen(sockfd, bufferFile, length * sizeof(char)), "write", "");

    buffer = realloc(buffer, n*sizeof(char));
    if (!buffer) { perror("realloc"); fprintf(stderr, "Memoria esaurita....\n"); }


    SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read", "");
      //fprintf(stderr, "e fin qui\n");
    SYSCALL_EXIT("readn", notused, readn(sockfd, buffer, n * sizeof(char)), "read", "");
    buffer[n] = '\0';
    printf("result: %s\n", buffer);
  } else {
    fprintf(stderr, "file già esistente nel server\n");
  }
}

int EseguiComandoClientServer(NodoComando *tmp) {
  if(tmp->cmd == 'c') {
    fprintf(stderr, "file da rimuovere %s\n", tmp->name);
    removeFile(tmp->name);
    return 0;
  } else if(tmp->cmd == 'r') {
    fprintf(stderr, "sto eseguendo sta parte\n");
    void* buf;
    int sizebuff; //col size_t non va
    readFile(tmp->name, &buf, &sizebuff); //manca gestione errore
    if(savefiledir != NULL && buf != NULL)
      writeBufToDisk(savefiledir, tmp->name, buf, sizebuff);
    return 0;
  } else if(tmp->cmd == 'R') {
    fprintf(stderr, "comando readNFiles con n = %d\n", tmp->n);
    readNFiles(tmp->n, savefiledir);
    return 0;
  } else if(tmp->cmd == 'W') {
    fprintf(stderr, "comando W con parametro %s\n", tmp->name);
    writeFile(tmp->name);
  }
  //da qui solo se il comando è W

}

int openFile(const char* pathname, int flags) {
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

  add_to_current_time(2, 0, &abstime);
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
  int x = openConnection(SOCKNAME, 0, abstime); //da vedere se da errore
  fprintf(stderr, "sockfd: %ld, risultato openconnection %d\n", sockfd, x);


  while(q->len > 0) { //finchè ci sono richieste che il parser ha visto
    NodoComando *tmp = pop(&q);
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
  close(sockfd);
  return 0;
}
