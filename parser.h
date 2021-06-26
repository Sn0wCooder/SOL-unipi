
typedef struct _CodaComandi {
  char cmd;
  char* name;
  int n;
} NodoComando;

void printQueue(Queue *q);
void insert(Queue **q, char cmd, char* name, int n);
Queue* parser(int argc, char* argv[]);
