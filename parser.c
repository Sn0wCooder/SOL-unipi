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

#include "errors.c"


int main(int argc, char* argv[]) {
  int c;
  int index, sflags, lcount;
  char* next, login;
  while((c = getopt(argc, argv, "l:w:S")) != -1){
    switch(c){
        case 'l':
            printf("Sto guardando gli argomenti di -l\n");

            char* arg = malloc(sizeof(char) * strlen(optarg));
            strncpy(arg, optarg, strlen(optarg));

            char *save = NULL;
            char *token = strtok_r(arg, ",", &save); // Attenzione: l’argomento stringa viene modificato!
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
        case 'w':
            printf("guardo w %s\n", optarg);

            arg = malloc(sizeof(char) * strlen(optarg));
            strncpy(arg, optarg, strlen(optarg));

            //Visto che il secondo parametro, se presente, è sicuramente un numero ed è l'ultimo dell'optarg, tokenizzo la funzione
            //prendendo quindi come primo argomento la dirname e come num il numero dato, che sarà sicuramente l'ultimo della stringa

            save = NULL;
            token = strtok_r(arg, ",", &save);
            int num = atoi(&optarg[strlen(optarg) - 1]);
            printf("%s %d\n", token, num);

            free(arg);
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
