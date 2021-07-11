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
//#include <libc.h>
#include <sys/un.h>
#include <ctype.h>
#include <pthread.h>


int isNumber(const char* s) {
  if (s==NULL)
    return 1;
  if (strlen(s)==0)
    return 1;
  char* e = NULL;
  errno = 0;
  long val = strtol(s, &e, 10);
  val = 0;
  if (errno == ERANGE)
    return 2;    // overflow
  if (e != NULL && *e == (char)0) {
    return 1;   // successo
  }
  return 0;   // non e' un numero
}

void Pthread_mutex_lock(pthread_mutex_t *mtx) {
  int err;
  if ( ( err=pthread_mutex_lock(mtx)) != 0 ) {
    errno=err;
    perror("pthread_mutex_lock");
    exit(EXIT_FAILURE);
    //pthread_exit((void*)errno);
  }
   //else printf(”locked ”);
}

void Pthread_mutex_unlock(pthread_mutex_t *mtx) {
  int err;
  if ( ( err=pthread_mutex_unlock(mtx)) != 0 ) {
     errno=err;
     perror("pthread_mutex_unlock");
     exit(EXIT_FAILURE);
     //pthread_exit(errno);
  }
   //else printf(”unlocked ”);
}
