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
#include "parser.h"
#include "util.h"

void printQueue(Queue *q) { //stampa la coda dei comandi
  Node* tmp = q->head;
  NodoComando *no = NULL;
  while(tmp != NULL) { //per ogni comando
    no = tmp->data;
    fprintf(stdout, "Comando %c, nome %s, n %d\n", no->cmd, no->name, no->n);
    tmp = tmp->next;
  }
}

void insert(Queue **q, char cmd, char* name, int n) { //crea un NodoComando e lo mette nella coda
  NodoComando *new; //nuovo comando
  ec_null((new = malloc(sizeof(NodoComando))), "malloc");
  //inserimento dei valori nel comando
  new->cmd = cmd;
  if(name != NULL) {
    ec_null((new->name = malloc(sizeof(char) * strlen(name))), "malloc");
    strncpy(new->name, name, strlen(name));
  }
  new->n = n;
  ec_meno1((push(q, new)), "push");
}

Queue* parser(int argc, char* argv[]) {
  int c;
  savefiledir = NULL;
  timems = 0;
  verbose = 0;
  seenf = 0;
  seenp = 0;

  char* arg, *token, *save;
  Queue *q; //coda dei comandi che poi verrà restituita
  ec_null((q = malloc(sizeof(Queue))), "malloc");
  while((c = getopt(argc, argv, "hf:w:W:r:Rd:t:l:u:c:p")) != -1){
    switch(c){
        case 'h': { //messaggio di help
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
        case 'f': { //sockName a cui connettersi
          if(seenf == 1) { //ha già visto un comando 'f', che va specificato una volta sola: errore
            fprintf(stderr, "ERRORE: l'opzione -f va specificata una volta sola\n");
            exit(EXIT_FAILURE);
          }
          seenf = 1; //ha visto un comando 'f'
          ec_null((socknameconfig = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc");
          strcpy(socknameconfig, optarg);
          socknameconfig[strlen(optarg)] = '\0';
          break;
        }
        case 'w': { //scrittura di n file
            ec_null((arg = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc"); //argomento della w in una variabile temporanea
            strncpy(arg, optarg, strlen(optarg)); //lo copio in una variabile temporanea
            arg[strlen(optarg)] = '\0';

            //tokenizzo la stringa e vedo il numero di virgole
            save = NULL;
            token = strtok_r(arg, ",", &save);
            char* dirname;
            //dirname = primo token prima della prima virgola
            ec_null((dirname = malloc(sizeof(char) * (strlen(token) + 1))), "malloc");
            strcpy(dirname, token);
            dirname[strlen(token)] = '\0';
            int contavirgole = -1; //ci dovrebbe essere solo una virgola
            char* tmp; //temporanea, in cui sarà salvato l'ultimo token dopo le virgole
            while(token) {
              contavirgole++;
              tmp = token;
              token = strtok_r(NULL, ",", &save);
            }

            int num;
            if(contavirgole > 1) {
              fprintf(stderr, "Errore: troppi argomenti\n");
              exit(EXIT_FAILURE);
            }
            else if(contavirgole == 1) { //fin qui ok, una sola virgola
              if(isNumber(tmp)) //controlla che sia un numero
                num = atoi(tmp); //lo converte in intero
              else {
                fprintf(stderr, "Errore: richiesto un numero\n");
                exit(EXIT_FAILURE);
              }
            } else { //contavirgole == 0
              num = 0; //valore di default
            }

            insert(&q, 'w', dirname, num); //inserisce il comando nella coda dei comandi

            free(arg);
            break;
          }
        case 'W': { //scrittura di uno o più file
            ec_null((arg = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc"); //argomento della funzione 'W'
            strncpy(arg, optarg, strlen(optarg)); //lo copia dall'optarg
            arg[strlen(optarg)] = '\0'; //inserisce il terminatore

            save = NULL;
            token = strtok_r(arg, ",", &save); //tokenizza con la virgola
            while(token) { //per ogni elemento separato da una virgola
              insert(&q, 'W', token, 0); //lo inserisco nella coda dei comandi
              token = strtok_r(NULL, ",", &save);
            }
            free(arg);
            break;
          }
          case 'r': { //lettura di uno o più file
              ec_null((arg = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc"); //argomento della funzione 'r'
              strncpy(arg, optarg, strlen(optarg)); //lo copia dall'optarg
              arg[strlen(optarg)] = '\0'; //inserisce il terminator

              save = NULL;
              token = strtok_r(arg, ",", &save); //tokenizza con la virgola
              while(token) { //per ogni elemento separato da una virgola
                token[strlen(token)] = '\0';
                insert(&q, 'r', token, 0); //lo inserisco nella coda dei comandi
                token = strtok_r(NULL, ",", &save);
              }
              free(arg);
              seenr = 1;
              break;
            }


        case 'R': { //lettura di N file
            //R può avere opzionalmente una opzione, che è messa quindi facoltativa e parsata a parte
            int nfacoltativo = 0; //n facoltativo
            char* nextstring = NULL; //la stringa seguente a -R passata da riga di comando
            if(optind != argc) { //se -R non è l'ultimo argomento passato
              nextstring = strdup(argv[optind]);
            }
            if(nextstring != NULL && nextstring[0] != '-') { //in questo if nextstring non è un comando, bisogna controllare se è un numero o una stringa
              if(isNumber(nextstring)) { //se è un numero
                nfacoltativo = atoi(nextstring); //aggiorna nfacoltativo
              }
              else { //non è un numero né un parametro, deve dare errore
                fprintf(stderr, "Errore: richiesto un numero\n");
                exit(EXIT_FAILURE);
              }
            }
            seenR = 1;
            insert(&q, 'R', NULL, nfacoltativo);
            break;
        }
        case 'd': { //directory locale in cui andare a salvare i file ottenuti dalle letture dal server
          ec_null((savefiledir = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc"); //alloca l'argomento
          strcpy(savefiledir, optarg); //copia l'optarg
          savefiledir[strlen(optarg)] = '\0'; //inserisce il terminatore
          break;
        }
        case 't': { //tempo in millisecondi tra due richieste successive
          timems = atoi(optarg);
          break;
        }
        case 'c': { //rimozione di uno o più file dal server
            ec_null((arg = malloc(sizeof(char) * (strlen(optarg) + 1))), "malloc"); //alloca l'argomento
            strncpy(arg, optarg, strlen(optarg)); //copia l'optarg
            arg[strlen(optarg)] = '\0'; //inserisce il terminatore

            save = NULL;
            token = strtok_r(arg, ",", &save); //tokenizza con la virgola
            while(token) { //per ogni elemento separato da una virgola
              insert(&q, 'c', token, 0); //inserisce il comando nella coda comandi
              token = strtok_r(NULL, ",", &save);
            }
            free(arg);
            break;
          }
          case 'p': { //modalità verbosa
            if(seenp == 1) { //avevo già visto unn 'p', errore
              fprintf(stderr, "ERRORE: l'opzione -p va specificata una volta sola\n");
              exit(EXIT_FAILURE);
            }
            seenp = 1;
            verbose = 1;
            break;
          }
        case ':':
            fprintf(stderr, "Errore: l'opzione -%c richiede un argomento\n", optopt);
            break;
        case '?':
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
