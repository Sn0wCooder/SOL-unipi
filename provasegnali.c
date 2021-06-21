#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <signal.h>

volatile sig_atomic_t sighup;
volatile sig_atomic_t sigquit;

void sighandlerquit (int sig) {
    write(1, "segnale SIGQUIT ricevuto\n", 24);
    //printf("ziobello\n");
    sigquit = 1;
    sighup = 0;
    return;
}

void sighandlerhup (int sig) {
    write(1, "segnale SIGHUP ricevuto", 25);
    //printf("ziobello\n");
    #ifdef DEBUG6
    //pthread_mutex_lock(&mutex_clienti);
    //printf("clienti = %d\n", num_clienti);
    //pthread_mutex_unlock(&mutex_clienti);
    #endif
    sigquit = 0;
    sighup = 1;
    return;
}

int main(void) {

  struct sigaction sahup;
  struct sigaction saquit;
  memset(&sahup, 0, sizeof(sigaction));
  memset(&saquit, 0, sizeof(sigaction));
  sahup.sa_handler = sighandlerhup;
  saquit.sa_handler = sighandlerquit;
  sigaction(SIGHUP, &sahup, NULL);
  sigaction(SIGQUIT, &saquit, NULL);
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGHUP);
  sigaddset (&mask, SIGQUIT);
  signal(SIGINT, sighandlerhup);
  sleep(10);
}
