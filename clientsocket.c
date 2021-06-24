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

#define SOCKNAME "ProvaSock.sk"

#define SYSCALL_EXIT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror(#name);				\
	int errno_copy = errno;			\
	exit(errno_copy);			\
    }


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


int main(int argc, char *argv[]) {
  if (argc == 1) {
	   fprintf(stderr, "usa: %s stringa [stringa]\n", argv[0]);
	    exit(EXIT_FAILURE);
  }

  struct sockaddr_un serv_addr;
  int sockfd;
  SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
  memset(&serv_addr, '0', sizeof(serv_addr));

  serv_addr.sun_family = AF_UNIX;
  strncpy(serv_addr.sun_path,SOCKNAME, strlen(SOCKNAME)+1);

  int notused;
  SYSCALL_EXIT("connect", notused, connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "connect", "");

  char *buffer=NULL;
  for(int i=1; i<argc;++i) {

	   int n=strlen(argv[i])+1;
	/*
	 *  NOTA: provare ad utilizzare writev (man 2 writev) per fare un'unica SC
	 */
	  SYSCALL_EXIT("writen", notused, writen(sockfd, &n, sizeof(int)), "write", "");
	  SYSCALL_EXIT("writen", notused, writen(sockfd, argv[i], n*sizeof(char)), "write", "");

	  buffer = realloc(buffer, n*sizeof(char));
	  if (!buffer) {
	   perror("realloc");
	   fprintf(stderr, "Memoria esaurita....\n");
	   break;
	  }
	/*
	 *  NOTA: provare ad utilizzare readv (man 2 readv) per fare un'unica SC
	 */
	  SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read","");
	  SYSCALL_EXIT("readn", notused, readn(sockfd, buffer, n*sizeof(char)), "read","");
	  buffer[n] = '\0';
	  printf("result: %s\n", buffer);
  }
  close(sockfd);
  if (buffer)
    free(buffer);
  return 0;
}
