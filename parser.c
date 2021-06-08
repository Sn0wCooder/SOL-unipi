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
#include <libc.h>
#include <sys/un.h>
#include <ctype.h>

#include "errors.c"

int isNumber(const char* s) {
  if (s==NULL) return 1;
  if (strlen(s)==0) return 1;
  char* e = NULL;
  errno=0;
  long val = strtol(s, &e, 10);
  if (errno == ERANGE) return 2;    // overflow
  if (e != NULL && *e == (char)0) {
    return 1;   // successo
  }
  return 0;   // non e' un numero
}

int main(int argc, char* argv[]) {
  int c;
  int index, sflags, lcount;
  char* next, login;
  char* arg, *token, *save;
  while((c = getopt(argc, argv, "f:w:W:R:S")) != -1){
    switch(c){
        case 'f':
          printf("filename %s\n", optarg);
          break;
        case 'w':
            printf("guardo w %s\n", optarg);
            //controllare: se ci sono più dirname, se c'è n e se n è un numero

            arg = malloc(sizeof(char) * strlen(optarg)); //argomento di w
            strncpy(arg, optarg, strlen(optarg)); //lo copio in una variabile temporanea

            //tokenizzo la stringa e vedo il numero di virgole
            save = NULL;
            token = strtok_r(arg, ",", &save);
            char* dirname = malloc(sizeof(char) * strlen(token)); //dirname = primo token prima della prima virgola
            strncpy(dirname, token, strlen(token));
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
            printf("%s %d\n", dirname, num);

            free(arg);
            break;
        case 'W':
            printf("Sto guardando gli argomenti di -l\n");

            arg = malloc(sizeof(char) * strlen(optarg));
            strncpy(arg, optarg, strlen(optarg));

            save = NULL;
            token = strtok_r(arg, ",", &save); // Attenzione: l’argomento stringa viene modificato!
            while(token) {
              printf("%s\n", token);
              token = strtok_r(NULL, ",", &save);
            }

                /*index = optind - 1;
                while(index < argc){
                    next = strdup(argv[index]); /* get login
                    index++;

                    if(next[0] != '-'){         // check if optarg is next switch
                        //login[lcount++] = next;
                        printf("%s\n", next);
                    }
                    else break;
                }
                optind = index - 1;*/
            free(arg);
            break;
        case 'R':
            //sflag++;                        /* other option */

            printf("caso R %s\n", optarg);
            break;
        case 'S':
                //sflag++;                        /* other option */
            printf("caso S\n");
            break;
        case ':':                           /* error - missing operand */
            fprintf(stderr, "Option -%c requires an operand\n", optopt);
            break;
        case '?':                           /* error - unknown option */
            fprintf(stderr,"Unrecognized option: -%c\n", optopt);
            break;
      }
   }
}
