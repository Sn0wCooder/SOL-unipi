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

//#include "errors.c"
#include "queue.h"
#include "parser.h"
#include "util.h"

/*typedef struct _CodaComandi {
  char cmd;
  char* name;
  int n;
} NodoComando;*/

void printQueue(Queue *q) {
  Node* tmp = q->head;
  NodoComando *no = NULL;
  while(tmp != NULL) {
    no = tmp->data;
    fprintf(stdout, "Comando %c, nome %s, n %d\n", no->cmd, no->name, no->n);
    tmp = tmp->next;
  }
}

void insert(Queue **q, char cmd, char* name, int n) { //crea il NodoComando e lo mette nella coda
  //printf("sto inserendo %c stringa %s n %d\n", cmd, name, n);
  NodoComando *new; // = malloc(sizeof(NodoComando));
  ec_null((new = malloc(sizeof(NodoComando))), "malloc");
  new->cmd = cmd;
  //printf("inserisco name %s\n", name);
  if(name != NULL) {
    ec_null((new->name = malloc(sizeof(char) * strlen(name))), "malloc");
    //new->name = malloc(sizeof(char) * strlen(name));
    strncpy(new->name, name, strlen(name));
  }
  new->n = n;
  ec_meno1((push(q, new)), "push");
  //push(q, new);
  //printf("\n\n\n");
  //printQueue(*q);
  //printf("\n\n\n");
}

/*int isNumber(const char* s) {
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
}*/

/*int isNumber1 (char* s) {
  int ok = 1;
  int len = strlen(s);
  int i = 0;
  while(ok && i < len) {
    if(!isdigit(s[i]))
      ok = 0;
    i++;
  }
  return ok;*/




  /**for (i=0;i<length; i++)
        if (!isdigit(input[i]))
        {
            printf ("Entered input is not a number\n");
            exit(1);
        }
}*/

Queue* parser(int argc, char* argv[]) {
  int c;
  //int index, sflags, lcount;
  //char* next, login;
  savefiledir = NULL;
  timems = 0;
  verbose = 0;
  seenf = 0;
  seenp = 0;

  char* arg, *token, *save;
  Queue *q; // = malloc(sizeof(Queue));
  ec_null((q = malloc(sizeof(Queue))), "malloc");
  while((c = getopt(argc, argv, "hf:w:W:r:Rd:t:l:u:c:p")) != -1){
    switch(c){
        case 'h': {
          fprintf(stdout, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
                    "-h                 : stampa la lista delle opzioni",
                    "-f filename        : specifica il filename del socket a cui deve connettersi il client",
                    "-w dirname[,n=0]   : manda i file dalla cartella 'dirname' al server",
                    "-W file1[,file2]   : lista dei file che devono essere scritti nel server",
                    "-r file1[,file2]   : lista dei file che devono essere letti dal server",
                    "-R [n=0]           : leggi 'n' file presenti sul server",
                    "-d dirname         : specifica la cartella dove scrivere i file letti con -r e -R",
                    "-t time            : tempo in millisecondi per fare due richieste consecutive al server",
                    "-c file1[,file2]   : lista di file da eliminare dal server, se presenti",
                    "-p                 : modalità verbosa per nerd"
            );
            exit(EXIT_SUCCESS);
            break;
        }
        case 'f': {

          if(seenf == 1) {
            fprintf(stderr, "ERRORE: l'opzione -f va specificata una volta sola\n");
            exit(EXIT_FAILURE);
          }
          seenf = 1;
          //insert(&q, 'f', optarg, 0);
          ec_null((socknameconfig = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
          //socknameconfig = malloc(sizeof(char) * strlen(optarg));
          strcpy(socknameconfig, optarg);
          socknameconfig[strlen(optarg)] = '\0';
          //printf("filename %s\n", optarg);
          //printQueue(q);
          break;
        }
        case 'w': {
            //printf("guardo w %s\n", optarg);
            //controllare: se ci sono più dirname, se c'è n e se n è un numero

            ec_null((arg = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
            //arg = malloc(sizeof(char) * strlen(optarg)); //argomento di w
            strncpy(arg, optarg, strlen(optarg)); //lo copio in una variabile temporanea
            arg[strlen(optarg)] = '\0';

            //tokenizzo la stringa e vedo il numero di virgole
            save = NULL;
            token = strtok_r(arg, ",", &save);
            char* dirname; // = malloc(sizeof(char) * strlen(token));
            //dirname = primo token prima della prima virgola
            ec_null((dirname = malloc(sizeof(char) * (strlen(token) + 1))), "malloc");
            strncpy(dirname, token, strlen(token));
            dirname[strlen(token)] = '\0';
            int contavirgole = -1;
            char* tmp; //temporanea, in cui sarà salvato l'ultimo token dopo le virgole
            while(token) {
              contavirgole++;
              tmp = token;
              token = strtok_r(NULL, ",", &save);
            }

            int num;
            if(contavirgole > 1) {
              fprintf(stderr, "FATAL ERROR: too many arguments\n");
              exit(EXIT_FAILURE);
            }
            else if(contavirgole == 1) {
              //controllare se l'ultimo token è un numero
              //fprintf(stderr, "tmp da vedere %s\n", tmp);
              if(isNumber(tmp))
                num = atoi(tmp);
              else {
                fprintf(stderr, "FATAL ERROR: number is required\n");
                exit(EXIT_FAILURE);
              }
            } else { //contavirgole == 0
              num = 0;
            }

            //int num = atoi(&optarg[strlen(optarg) - 1]);
            insert(&q, 'w', dirname, num);
            //printf("%s %d\n", dirname, num);
            //printQueue(q);

            free(arg);
            break;
          }
        case 'W': {
            //printf("Sto guardando gli argomenti di -l\n");

            ec_null((arg = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
            //arg = malloc(sizeof(char) * strlen(optarg));
            strncpy(arg, optarg, strlen(optarg));
            arg[strlen(optarg)] = '\0';

            save = NULL;
            token = strtok_r(arg, ",", &save); // Attenzione: l’argomento stringa viene modificato!
            while(token) {
              //printf("inserisco %s\n", token);
              insert(&q, 'W', token, 0);
              token = strtok_r(NULL, ",", &save);
            }
            free(arg);
            //printQueue(q);
            //printf("\n\n\n");
            break;
          }
          case 'r': {
              //printf("Sto guardando gli argomenti di -l\n");
              //fprintf(stderr, "argomento %s\n", optarg);

              ec_null((arg = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
              //arg = malloc(sizeof(char) * strlen(optarg));
              strncpy(arg, optarg, strlen(optarg));
              arg[strlen(optarg)] = '\0';

              save = NULL;
              token = strtok_r(arg, ",", &save); // Attenzione: l’argomento stringa viene modificato!
              while(token) {
                token[strlen(token)] = '\0';
                //printf("inserisco %s\n", token);
                insert(&q, 'r', token, 0);
                token = strtok_r(NULL, ",", &save);
              }
              free(arg);
              seenr = 1;
              //printQueue(q);
              //printf("\n\n\n");
              break;
            }


        case 'R': {
            //R può avere opzionalmente una opzione, che è messa quindi facoltativa e parsata a parte
            //printf("guardo R\n");
            int nfacoltativo = 0;
            char* nextstring = NULL; //la stringa seguente a -R passata da riga di comando
            if(optind != argc) { //se -R non è l'ultimo argomento passato
              //fprintf(stderr, "optind %d\n",optind);
              nextstring = strdup(argv[optind]);
            }
            //fprintf(stderr, "fin qui tutto OKKKKKK\n");
            if(nextstring != NULL && nextstring[0] != '-') { //nextstring non è un comando, bisogna controllare se è un numero o una stringa
              if(isNumber(nextstring)) {
                nfacoltativo = atoi(nextstring);
              }
              else { //non è un numero né un parametro, deve dare errore
                fprintf(stderr, "FATAL ERROR: number is required\n");
                exit(EXIT_FAILURE);
              }
            }


            //printQueue(q);
            //printf("\n\n\n");
            seenR = 1;
            insert(&q, 'R', NULL, nfacoltativo);
            //sleep(1);
            //printf("caso R %d\n", nfacoltativo);
            //printQueue(q);
            break;
        }
        case 'd': {
          //fprintf(stderr, "siamo alla d\n");
          //insert(&q, 'd', optarg, 0);
          ec_null((savefiledir = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
          //savefiledir = malloc(sizeof(char) * strlen(optarg));
          strcpy(savefiledir, optarg);
          savefiledir[strlen(optarg)] = '\0';
          //printf("filename %s\n", optarg);
          //printQueue(q);
          break;
        }
        case 't': {
          //insert(&q, 't', optarg, 0);
          //printf("filename %s\n", optarg);
          timems = atoi(optarg);
          //printQueue(q);
          break;
        }
        /*case 'l': {
          insert(&q, 'l', optarg, 0);
          //printf("filename %s\n", optarg);
          //printQueue(q);
          break;
        }
        case 'u': {
          insert(&q, 'u', optarg, 0);
          //printf("filename %s\n", optarg);
          //printQueue(q);
          break;
        }*/
        case 'c': {
            //printf("Sto guardando gli argomenti di -l\n");

            ec_null((arg = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
            //arg = malloc(sizeof(char) * strlen(optarg));
            strncpy(arg, optarg, strlen(optarg));
            arg[strlen(optarg)] = '\0';

            save = NULL;
            token = strtok_r(arg, ",", &save); // Attenzione: l’argomento stringa viene modificato!
            while(token) {
              //printf("inserisco %s\n", token);
              insert(&q, 'c', token, 0);
              token = strtok_r(NULL, ",", &save);
            }
            free(arg);
            //printQueue(q);
            //printf("\n\n\n");
            break;
          }
          case 'p': {
            //fprintf(stderr, "p identificato\n");
            //insert(&q, 'p', optarg, 0);
            //printf("filename %s\n", optarg);
            //printQueue(q);
            if(seenp == 1) {
              fprintf(stderr, "ERRORE: l'opzione -p va specificata una volta sola\n");
              exit(EXIT_FAILURE);
            }
            seenp = 1;
            verbose = 1;
            //printQueue(q);
            break;
          }
        /*case 'S':
                //sflag++;                        other option
            printf("caso S\n");
            break;*/
        case ':':                           /* error - missing operand */
            fprintf(stderr, "Errore: l'opzione -%c richiede un argomento\n", optopt);
            break;
        case '?':                           /* error - unknown option */
            fprintf(stderr,"Opzione non riconosciuta: -%c\n", optopt);
            break;
      }
   }
   if(savefiledir != NULL && seenr == 0 && seenR == 0) {
     fprintf(stderr, "Errore, l'opzione -d va usata insieme a -r o a -R\n");
     exit(EXIT_FAILURE);
   }
   return q;
}
