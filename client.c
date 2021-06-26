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
long sockfd;


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
    int sockfd;
    SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path,SOCKNAME, strlen(SOCKNAME)+1);

    int notused;
    SYSCALL_EXIT("connect", notused, connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "connect", "");










    /*// setting waiting time
    struct timespec wait_time;
    // no need to check because msec > 0 and &wait_time != NULL
    set_timespec_from_msec(msec, &wait_time);

    // setting current time
    struct timespec curr_time;
    clock_gettime(CLOCK_REALTIME, &curr_time);

    //fprintf(stderr, "fin qui");
    sleep(3);
    // trying to connect
    int err = -1;
    while( (err = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) == -1
            && curr_time.tv_sec < abstime.tv_sec ){
        //debug("connect didn't succeed, trying again...\n");
        fprintf(stderr, "err4\n");

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
    }*/

    //debug("Connected! :D\n");

    //socket_path = sockname;
    return 0;
}

int main(int argc, char *argv[]) {
  //Queue *q = parser(argc, argv); //coda delle operazioni
  //struct timespec abstime;

  //add_to_current_time(config.waiting_sec, 0, &abstime);

  struct sockaddr_un serv_addr;
  int sockfd;
  SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
  memset(&serv_addr, '0', sizeof(serv_addr));

  serv_addr.sun_family = AF_UNIX;
  strncpy(serv_addr.sun_path,SOCKNAME, strlen(SOCKNAME)+1);

  int notused;
  SYSCALL_EXIT("connect", notused, connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "connect", "");
  //int x = openConnection(SOCKNAME, 0, abstime); //da vedere se da errore
  //fprintf(stderr, "fdsock: %ld, risultato %d\n", sockfd, x);
  //while(q->len > 0) { //finchè ci sono richieste che il parser ha visto

  //}

  return 0;
}
