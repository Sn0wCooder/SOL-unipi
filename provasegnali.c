#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <signal.h>

volatile sig_atomic_t sighup;   //variabile globale, definita così perché con i segnali non è garantito l'accesso safe alle variabili globali definite "normalmente"
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

int threadworker() {

}

int threadgestore() {
  int segnalericevuto;
sigwait(mask, &segnalericevuto);
//arrivo qui: ho ricevuto un segnale
if(segnalericevuto == SIGINT) ...
}

int main(void) {

  struct sigaction sahup;
  struct sigaction saquit;
  struct sigaction saint; //control+C
  memset(&sahup, 0, sizeof(sigaction));
  memset(&saquit, 0, sizeof(sigaction));
  memset(&saint, 0, sizeof(sigaction));
  sahup.sa_handler = sighandlerhup;
  saquit.sa_handler = sighandlerquit;
  saint.sa_handler = sighandlerquit;
  /*sigaction(SIGHUP, &sahup, NULL);
  sigaction(SIGQUIT, &saquit, NULL);
  sigaction(SIGINT, &saint, NULL);*/

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGHUP);
  sigaddset (&mask, SIGQUIT);
  sigaddset (&mask, SIGINT);
  pthread_sigmask(SIG_SETMASK, &mask, NULL);
  //spawn thread worker
  //spawn thread gestore, con la sigwait

  //signal(SIGINT, sighandlerhup);
  sleep(10);
}
