#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

enum {
  NPREF = 2,       /* number of prefix words */
  NHASH = 4096,    /* size of the state hash table array */
  MAXGEN = 10000,  /* maximum words generated */
};

char NONWORD[] = "\n"; /* cannot appear as a real word */

typedef struct State State;
typedef struct Suffix Suffix;

struct State {
  char *pref[NPREF]; /* prefix words */
  Suffix *suf;       /* list of suffixes */
  State *next;       /* next in hash table */
};

struct Suffix {
  char *word;   /* suffix */
  Suffix *next; /* suffix */
};

State *statetab[NHASH]; /* hash table of states */

/* make GCC happy with std=c89, but GCC still provides it */
char *strdup(char *s);

/* hash: compute hash value for array of NPREF strings */
unsigned int hash(char *s[NPREF]) {
  unsigned int h;
  unsigned char *p;
  int i;

  h = 0;
  for (i = 0; i < NPREF; i++) {
    for (p = (unsigned char *) s[i]; *p != '\0'; p++) {
      h = 31 * h + *p;
    }
  }
  return h % NHASH;
}

/* returns pointer if present or created, NULL if not */
/* creation doesn't strdup so strings mustn't change later. */
State* lookup(char *prefix[NPREF], int create) {
  int i, h;
  State *sp;

  h = hash(prefix);

  for (sp = statetab[h]; sp != NULL; sp = sp->next) {
    for (i = 0; i < NPREF; i++) {
      if (strcmp(prefix[i], sp->pref[i]) != 0) {
        break;
      }
    }
    if (i == NPREF) /* found it */
      return sp;
  }

  if (create) {
    sp = (State *) malloc(sizeof(State));
    for (i = 0; i < NPREF; i++) {
      sp->pref[i] = prefix[i];
    }

    sp->suf = NULL;
    sp->next = statetab[h];
    statetab[h] = sp;
  }

  return sp;
}

/* add to state, suffix must not change later */
void addsuffix(State *sp, char *suffix) {
  Suffix *suf;

  suf = (Suffix *) malloc(sizeof(Suffix));
  suf->word = suffix;
  suf->next = sp->suf;
  sp->suf = suf;
}

/* add word to suffix list, update prefix */
void add(char *prefix[NPREF], char *suffix) {
  State *sp;

  sp = lookup(prefix, 1); /* create if not found */
  addsuffix(sp, suffix);

  /* pop the first word from the prefix */
  memmove(prefix /* dest */, prefix+1 /* src */, (NPREF-1)*sizeof(prefix[0]));
  prefix[NPREF-1] = suffix;
}

/* read input, build prefix table */
void build(char *startPrefix[NPREF], char *prefix[NPREF], FILE *f) {
  char buf[100], fmt[10];

  int i, nstartPref = 0;

  /* create format string; %s could overflow buf */
  sprintf(fmt, "%%%zus", sizeof(buf)-1); /* minus one so sprintf has room for the \0 */
  while (fscanf(f, fmt, buf) != EOF) {
    /* TODO estrdup */
    add(prefix, strdup(buf));

    /* simple random choice of initial prefix that is the likely the start of a
     * sentence, again based on reservoir sampling */
    char first = prefix[0][0];
    if (first >= 'A' && first <= 'Z') {
      if (rand() % ++nstartPref == 0) {
        for (i = 0; i < NPREF; i++) {
          startPrefix[i] = prefix[i];
        }
      }
    }
  }
}

/* produce output, one word per line */
void generate(char *prefix[NPREF], int nwords) {
  State *sp;
  Suffix *suf;
  char *w;
  int i, nmatch;

  for (i = 0; i < NPREF; i++) {
    printf("%s ", prefix[i]);
  }

  for (i = 0; i < nwords; i++) {
    sp = lookup(prefix, 0);
    nmatch = 0;
    for (suf = sp->suf; suf != NULL; suf = suf->next) {
      /* See https://en.wikipedia.org/wiki/Reservoir_sampling */
      /* We test for exact divisibility whch is less likely as the nmatch
       * number grows as on average it will be true once every X iterations,
       * where X equals nmatch, so 1/nmatch, so 1/1, 1/2, 1/3, ... */
      if (rand() % ++nmatch == 0) { /* prob = 1/nmatch */
        w = suf->word;
      }
    }
    if (strcmp(w, NONWORD) == 0) {
      break;
    }
    printf("%s ", w);
    memmove(prefix, prefix+1, (NPREF-1)*sizeof(prefix[0]));
    prefix[NPREF-1] = w;
  }
  puts("\n");
}

int main(int argc, char *argv[]) {
  {
    /* seed the random number generator */
    /* int seed = time(NULL); */
    /* srand(seed); */

    /* more than once a second */
    struct timeval time; 
    gettimeofday(&time,NULL);
    srand((time.tv_sec * 1000) + (time.tv_usec / 1000));
  }

  int i;
  char *prefix[NPREF]; /* current input prefix */
  char *startPrefix[NPREF]; /* prefix to start generating from */
  for (i = 0; i < NPREF; i++) { /* set up initial prefix */
    startPrefix[i] = prefix[i] = NONWORD;
  }

  build(startPrefix, prefix, stdin);

  /* add a marker word used to terminate the generate algorithm */
  add(prefix, NONWORD);

  if (argc < 2) {
    fprintf(stderr, "specify the number of words\n");
    return 1;
  }
  generate(startPrefix, atoi(argv[1]));
  return 0;
}
