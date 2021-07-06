#include <stdio.h>
#include <stdlib.h>

int main(void) {
  FILE * f = fopen ("Instagram_193.0_Telefonbuchios14ok.ipa", "rb");
  long length;
  char* buffer;

  if (f) {
    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = malloc (length);
    if (buffer)
    {
      fread (buffer, 1, length, f);
    }
    fclose (f);
  }

  fprintf(stderr, "ciao\n");

  //for(int i = 0; i < length; i++)
    //printf("%s", buffer);

  FILE *f2 = fopen ("instagram.ipa", "w");
  //fprintf(f2, "%s", buffer);
  fwrite(buffer, 1, length, f2);

  fclose(f2);

  return 0;
}

void main1(int argc, char **argv)
{
    FILE *f = fopen("/dev/null", "r");
    fprintf(stderr, "e fin qui\n");
    int i;
    int written = 0;
    char *buf = malloc(100000);
    fprintf(stderr, "e fin qui3\n");

    setbuffer(f, buf, 100000);
    fprintf(stderr, "e fin qui2\n");
    for (i = 0; i < 1000; i++)
    {
        written += fprintf(f, "Number %d %c\n", i, buf[i]);
    }
    for (i = 0; i < written; i++) {
        printf("%c", buf[i]);
    }
}
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  FILE * f = fopen ("Instagram_193.0_Telefonbuchios14ok.ipa", "rb");
  long length;
  char* buffer;

  if (f) {
    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = malloc (length);
    if (buffer)
    {
      fread (buffer, 1, length, f);
    }
    fclose (f);
  }

  fprintf(stderr, "ciao\n");

  //for(int i = 0; i < length; i++)
    //printf("%s", buffer);

  FILE *f2 = fopen ("instagram.ipa", "w");
  //fprintf(f2, "%s", buffer);
  fwrite(buffer, 1, length, f2);

  fclose(f2);

  return 0;
}

void main1(int argc, char **argv)
{
    FILE *f = fopen("/dev/null", "r");
    fprintf(stderr, "e fin qui\n");
    int i;
    int written = 0;
    char *buf = malloc(100000);
    fprintf(stderr, "e fin qui3\n");

    setbuffer(f, buf, 100000);
    fprintf(stderr, "e fin qui2\n");
    for (i = 0; i < 1000; i++)
    {
        written += fprintf(f, "Number %d %c\n", i, buf[i]);
    }
    for (i = 0; i < written; i++) {
        printf("%c", buf[i]);
    }
}
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  FILE * f = fopen ("Instagram_193.0_Telefonbuchios14ok.ipa", "rb");
  long length;
  char* buffer;

  if (f) {
    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = malloc (length);
    if (buffer)
    {
      fread (buffer, 1, length, f);
    }
    fclose (f);
  }

  fprintf(stderr, "ciao\n");

  //for(int i = 0; i < length; i++)
    //printf("%s", buffer);

  FILE *f2 = fopen ("instagram.ipa", "w");
  //fprintf(f2, "%s", buffer);
  fwrite(buffer, 1, length, f2);

  fclose(f2);

  return 0;
}

void main1(int argc, char **argv)
{
    FILE *f = fopen("/dev/null", "r");
    fprintf(stderr, "e fin qui\n");
    int i;
    int written = 0;
    char *buf = malloc(100000);
    fprintf(stderr, "e fin qui3\n");

    setbuffer(f, buf, 100000);
    fprintf(stderr, "e fin qui2\n");
    for (i = 0; i < 1000; i++)
    {
        written += fprintf(f, "Number %d %c\n", i, buf[i]);
    }
    for (i = 0; i < written; i++) {
        printf("%c", buf[i]);
    }
}
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  FILE * f = fopen ("Instagram_193.0_Telefonbuchios14ok.ipa", "rb");
  long length;
  char* buffer;

  if (f) {
    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = malloc (length);
    if (buffer)
    {
      fread (buffer, 1, length, f);
    }
    fclose (f);
  }

  fprintf(stderr, "ciao\n");

  //for(int i = 0; i < length; i++)
    //printf("%s", buffer);

  FILE *f2 = fopen ("instagram.ipa", "w");
  //fprintf(f2, "%s", buffer);
  fwrite(buffer, 1, length, f2);

  fclose(f2);

  return 0;
}

void main1(int argc, char **argv)
{
    FILE *f = fopen("/dev/null", "r");
    fprintf(stderr, "e fin qui\n");
    int i;
    int written = 0;
    char *buf = malloc(100000);
    fprintf(stderr, "e fin qui3\n");

    setbuffer(f, buf, 100000);
    fprintf(stderr, "e fin qui2\n");
    for (i = 0; i < 1000; i++)
    {
        written += fprintf(f, "Number %d %c\n", i, buf[i]);
    }
    for (i = 0; i < written; i++) {
        printf("%c", buf[i]);
    }
}
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  FILE * f = fopen ("Instagram_193.0_Telefonbuchios14ok.ipa", "rb");
  long length;
  char* buffer;

  if (f) {
    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = malloc (length);
    if (buffer)
    {
      fread (buffer, 1, length, f);
    }
    fclose (f);
  }

  fprintf(stderr, "ciao\n");

  //for(int i = 0; i < length; i++)
    //printf("%s", buffer);

  FILE *f2 = fopen ("instagram.ipa", "w");
  //fprintf(f2, "%s", buffer);
  fwrite(buffer, 1, length, f2);

  fclose(f2);

  return 0;
}

void main1(int argc, char **argv)
{
    FILE *f = fopen("/dev/null", "r");
    fprintf(stderr, "e fin qui\n");
    int i;
    int written = 0;
    char *buf = malloc(100000);
    fprintf(stderr, "e fin qui3\n");

    setbuffer(f, buf, 100000);
    fprintf(stderr, "e fin qui2\n");
    for (i = 0; i < 1000; i++)
    {
        written += fprintf(f, "Number %d %c\n", i, buf[i]);
    }
    for (i = 0; i < written; i++) {
        printf("%c", buf[i]);
    }
}
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  FILE * f = fopen ("Instagram_193.0_Telefonbuchios14ok.ipa", "rb");
  long length;
  char* buffer;

  if (f) {
    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = malloc (length);
    if (buffer)
    {
      fread (buffer, 1, length, f);
    }
    fclose (f);
  }

  fprintf(stderr, "ciao\n");

  //for(int i = 0; i < length; i++)
    //printf("%s", buffer);

  FILE *f2 = fopen ("instagram.ipa", "w");
  //fprintf(f2, "%s", buffer);
  fwrite(buffer, 1, length, f2);

  fclose(f2);

  return 0;
}

void main1(int argc, char **argv)
{
    FILE *f = fopen("/dev/null", "r");
    fprintf(stderr, "e fin qui\n");
    int i;
    int written = 0;
    char *buf = malloc(100000);
    fprintf(stderr, "e fin qui3\n");

    setbuffer(f, buf, 100000);
    fprintf(stderr, "e fin qui2\n");
    for (i = 0; i < 1000; i++)
    {
        written += fprintf(f, "Number %d %c\n", i, buf[i]);
    }
    for (i = 0; i < written; i++) {
        printf("%c", buf[i]);
    }
}
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  FILE * f = fopen ("Instagram_193.0_Telefonbuchios14ok.ipa", "rb");
  long length;
  char* buffer;

  if (f) {
    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = malloc (length);
    if (buffer)
    {
      fread (buffer, 1, length, f);
    }
    fclose (f);
  }

  fprintf(stderr, "ciao\n");

  //for(int i = 0; i < length; i++)
    //printf("%s", buffer);

  FILE *f2 = fopen ("instagram.ipa", "w");
  //fprintf(f2, "%s", buffer);
  fwrite(buffer, 1, length, f2);

  fclose(f2);

  return 0;
}

void main1(int argc, char **argv)
{
    FILE *f = fopen("/dev/null", "r");
    fprintf(stderr, "e fin qui\n");
    int i;
    int written = 0;
    char *buf = malloc(100000);
    fprintf(stderr, "e fin qui3\n");

    setbuffer(f, buf, 100000);
    fprintf(stderr, "e fin qui2\n");
    for (i = 0; i < 1000; i++)
    {
        written += fprintf(f, "Number %d %c\n", i, buf[i]);
    }
    for (i = 0; i < written; i++) {
        printf("%c", buf[i]);
    }
}
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  FILE * f = fopen ("Instagram_193.0_Telefonbuchios14ok.ipa", "rb");
  long length;
  char* buffer;

  if (f) {
    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = malloc (length);
    if (buffer)
    {
      fread (buffer, 1, length, f);
    }
    fclose (f);
  }

  fprintf(stderr, "ciao\n");

  //for(int i = 0; i < length; i++)
    //printf("%s", buffer);

  FILE *f2 = fopen ("instagram.ipa", "w");
  //fprintf(f2, "%s", buffer);
  fwrite(buffer, 1, length, f2);

  fclose(f2);

  return 0;
}

void main1(int argc, char **argv)
{
    FILE *f = fopen("/dev/null", "r");
    fprintf(stderr, "e fin qui\n");
    int i;
    int written = 0;
    char *buf = malloc(100000);
    fprintf(stderr, "e fin qui3\n");

    setbuffer(f, buf, 100000);
    fprintf(stderr, "e fin qui2\n");
    for (i = 0; i < 1000; i++)
    {
        written += fprintf(f, "Number %d %c\n", i, buf[i]);
    }
    for (i = 0; i < written; i++) {
        printf("%c", buf[i]);
    }
}
