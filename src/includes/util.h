#include <sys/types.h>
     #include <sys/uio.h>
     #include <unistd.h>

int isNumber(const char* s);
void Pthread_mutex_lock(pthread_mutex_t *mtx); //funzione per lockare un mutex controllando gli errori
void Pthread_mutex_unlock(pthread_mutex_t *mtx); //funzione per unlockare un mutex controllando gli errori

static inline int writen(long fd, void *buf, size_t size); //evita scritture parziali
static inline int readn(long fd, void *buf, size_t size); //evita letture parziali

#define ec_meno1(c, s) if((c) == -1){ perror(s); exit(EXIT_FAILURE); }
#define neq_zero(c, s) if((c) != 0){ perror(s); exit(EXIT_FAILURE); }
#define ec_null(c, s) if((c) == NULL) { perror(s); exit(EXIT_FAILURE); }


#define SYSCALL_EXIT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	     perror(#name);				\
	      int errno_copy = errno;			\
	       exit(errno_copy);			\
    }

/** Evita letture parziali
*
*   \retval -1   errore (errno settato)
*   \retval  0   se durante la lettura la read ritorna 0
*   \retval  1   se la lettura termina con successo
*/
static inline int readn(long fd, void *buf, size_t size) {
  size_t left = size;
  int r;
  char *bufptr = (char*)buf;
  while(left > 0) {
    if ((r = read((int)fd, bufptr, left)) == -1) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (r == 0)
      return 0; //EOF
    left-= r;
    bufptr+= r;
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
  while(left > 0) {
    if ((r = write((int)fd, bufptr, left)) == -1) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (r == 0)
      return 0;
    left-= r;
    bufptr+= r;
  }
  return 1;
}
