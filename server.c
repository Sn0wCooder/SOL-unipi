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

//"nomeInfo valore"
//"spazio 1000"
//"numeroFIle 100"
#define MAXBUFFER 1000
#define MAXSTRING 100
#define SPAZIO "spazio"
#define NUMEROFILE "numeroFile"


int isNumber (char* s) {
  int ok = 1;
  int len = strlen(s);
  int i = 0;
  while(ok && i < len) {
    if(!isdigit(s[i]))
      ok = 0;
    i++;
  }
  return ok;
}

int main(int argc, char* argv[]) {
  char* a = NULL;
  int i;
  char* save;
  char* token;
  int spazio = 0;
  int numeroFile = 0;
  char* buffer = malloc(sizeof(char)*MAXBUFFER);
  FILE* p;
  //fprintf(stderr,"NON abbiamo aperto il file\n");
  if((p = fopen("config.txt", "r")) == NULL) {
        //gestione dell'errore
    perror("fopen");
  }
  //fprintf(stderr, "abbiamo aperto il file\n");
  while(fgets(buffer, MAXBUFFER, p)) {
    //fprintf(stderr, "ecco il buffer: %s\n", buffer);
        //ora facciamo il parser della singola riga

            //tokenizzo la stringa e vedo il numero di virgole
    save = NULL;
    token = strtok_r(buffer, " ",  &save);
    char** tmp = malloc(sizeof(char*) * 2);
    i = 0;
    while(token) {
      //fprintf(stderr, "sto processando %s\n", token);
      tmp[i] = malloc(sizeof(char) * MAXSTRING);
      strncpy(tmp[i], token, strlen(token) - i); //se è il secondo elemento (i = i) non deve prendere il newline finale,
      token = strtok_r(NULL, " ", &save);
      i++;
    }

    //fprintf(stderr, "Array: %s %s \n", tmp[0], tmp[1]);

    if(strcmp(tmp[0], SPAZIO) == 0) {
                //le due stringhe sono uguali
                //fprintf(stderr,"questo è il primo arg %s\n",tmp[1]);
      //fprintf(stderr,"questo è il risultato dello spazio %d\n", isNumber(tmp[1]));
      if(isNumber(tmp[1])) {
                    //fprintf(stderr,"questo è a %s\n",a);
        spazio = atoi(tmp[1]);
                    //fprintf(stderr,"questo è il risultato %d\n",spazio);
      } else fprintf(stderr, "ERRORE %s non è un numero\n", tmp[1]);
    }
    if(strcmp(tmp[0], NUMEROFILE) == 0) {
                //le due stringhe sono uguali
      //fprintf(stderr,"questo è il risultato del numero file %d\n", isNumber(tmp[1]));
      if(isNumber(tmp[1])) {
        numeroFile = atoi(tmp[1]);
      }
    }
  }
  fclose(p);



  //fprintf(stderr,"abbiamo chiuso il file\n");
  fprintf(stderr,"spazio: %d\n", spazio);
  fprintf(stderr,"numeroFile: %d\n", numeroFile);
}
