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


int main(int argc, char *argv[]) {
  if (argc == 1) {
	   fprintf(stderr, "usa: %s stringa [stringa]\n", argv[0]);
	    exit(EXIT_FAILURE);
  }

  //struct timespec abstime;
  //openConnection(SOCKNAME, 0, abstime);

  struct sockaddr_un serv_addr;
  SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
  memset(&serv_addr, '0', sizeof(serv_addr));

  serv_addr.sun_family = AF_UNIX;
  strncpy(serv_addr.sun_path,SOCKNAME, strlen(SOCKNAME)+1);

  int notused;
  SYSCALL_EXIT("connect", notused, connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "connect", "");

  //sleep(3);


  char *buffer=NULL;
  for(int i=1; i<argc;++i) {

	   int n=strlen(argv[i])+1;

	  SYSCALL_EXIT("writen", notused, writen(sockfd, &n, sizeof(int)), "write", "");
	  SYSCALL_EXIT("writen", notused, writen(sockfd, argv[i], n*sizeof(char)), "write", "");

	  buffer = realloc(buffer, n*sizeof(char));
	  if (!buffer) {
	   perror("realloc");
	   fprintf(stderr, "Memoria esaurita....\n");
	   break;
	  }

	  SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read","");
	  SYSCALL_EXIT("readn", notused, readn(sockfd, buffer, n*sizeof(char)), "read","");
	  buffer[n] = '\0';
	  printf("result: %s\n", buffer);
  }
  //close(sockfd);
  //if (buffer)
    //free(buffer);
  return 0;
}
