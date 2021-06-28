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

int EseguiComandoClientServer(NodoComando *tmp) {
  int notused;
  char *buffer = NULL;
  if(tmp == NULL) return -1; //errore: tmp non può e non deve essere NULL. Abbiamo già controllato che q->len > 0
  char* towrite = malloc(sizeof(char) * (strlen(tmp->name) + 1)); //alloco la stringa da scrivere, che sarà del tipo "rfile"
  towrite[0] = tmp->cmd;
  for(int i = 1; i <= strlen(tmp->name); i++)
    towrite[i] = tmp->name[i - 1];
  fprintf(stderr, "sto scrivendo nel socket %s\n", towrite);
  int n = strlen(towrite) + 1; //terminatore

  SYSCALL_EXIT("writen", notused, writen(sockfd, &n, sizeof(int)), "write", "");
  SYSCALL_EXIT("writen", notused, writen(sockfd, towrite, n * sizeof(char)), "write", "");

  if(tmp->cmd == 'W') { //comando di scrittura di un file

    buffer = realloc(buffer, n*sizeof(char));
    if (!buffer) { perror("realloc"); fprintf(stderr, "Memoria esaurita....\n"); }


    SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read", "");
    //fprintf(stderr, "e fin qui\n");
    SYSCALL_EXIT("readn", notused, readn(sockfd, buffer, n * sizeof(char)), "read", "");
    buffer[n] = '\0';
    fprintf(stderr, "buffer %s\n", buffer);
    if(strcmp(buffer, "file ok") == 0) {
      fprintf(stderr, "file ok\n");

      FILE * f = fopen (tmp->name, "rb");

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
      SYSCALL_EXIT("writen", notused, writen(sockfd, &length, sizeof(int)), "write", "");
      SYSCALL_EXIT("writen", notused, writen(sockfd, bufferFile, length * sizeof(char)), "write", "");

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
}

void visitaRicorsiva(char* path, int *n, Queue **q) {
  //char buf[MAXBUFFER];
  //if (getcwd(buf, MAXBUFFER) == NULL) { perror("getcwd");  exit(EXIT_FAILURE); }
  //if (chdir(path) == -1) { perror("chdir2");  exit(EXIT_FAILURE); }

  DIR *d;
  struct dirent* entry;
  if ((d = opendir(path)) == NULL) { perror("opening cwd");  exit(EXIT_FAILURE); }
  while ((errno = 0, entry = readdir(d)) != NULL) { //per ogni cosa in quella dir
    //struct stat info;
    fprintf(stderr, "sto processando %s\n", entry->d_name);
    //if (stat(file->d_name, &info) == -1){ perror("stat"); exit(EXIT_FAILURE); }
    if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") && strcmp(entry->d_name, ".git") != 0) {
      //fprintf(stderr, "%s\n", file->d_name);
      visitaRicorsiva(entry->d_name, n, q); //entriamo ricorsivamente nella dir
      //printf("");
    } else if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){

      if(*n > 0 || *n == -1) { //prende il file
        NodoComando *new = malloc(sizeof(NodoComando));
        new->cmd = 'W';
        new->name = entry->d_name;
        //push(q, new);
        fprintf(stderr, "file %s\n", new->name);
      }
      if(*n > 0)
        (*n)--;
      //printf("%s %lu ", file->d_name, (long)info.st_size);
    }


    //if (chdir(buf) == -1) { perror("chdir");  exit(EXIT_FAILURE); }

  }
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
    if(tmp->cmd == 'w') { //non fa una richiesta al server, ma visita ricorsivamente e fa una richiesta a parte per ogni file
      if(tmp->n == 0)
        tmp->n = -1;
      visitaRicorsiva(tmp->name, &(tmp->n), &q);
    } else
      EseguiComandoClientServer(tmp);

  }
  close(sockfd);
  return 0;
}
