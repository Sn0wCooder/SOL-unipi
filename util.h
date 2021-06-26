

int isNumber(const char* s);

static inline int writen(long fd, void *buf, size_t size);
static inline int readn(long fd, void *buf, size_t size);

#define SYSCALL_EXIT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror(#name);				\
	int errno_copy = errno;			\
	exit(errno_copy);			\
    }
