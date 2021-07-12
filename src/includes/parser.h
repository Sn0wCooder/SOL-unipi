typedef struct _CodaComandi { //coda dei comandi
  char cmd;
  char* name;
  int n;
} NodoComando;

void printQueue(Queue *q); //stampa la coda dei comandi
void insert(Queue **q, char cmd, char* name, int n); //inserisce il comando nella coda dei comandi
Queue* parser(int argc, char* argv[]); //funzione parser dei comandi

char* savefiledir;
int seenr; //ha visto un comando 'r'
int seenR; //ha visto un comando 'R'
int timems; //tempo in millisecondi tra due richieste successiva
char* socknameconfig; //sockname
int verbose; //per le stampe verbose
int seenf; //se ha visto un comando 'f'
int seenp; //se ha visto un comando 'p'
