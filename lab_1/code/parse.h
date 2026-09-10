typedef struct c
{
  char **pgmlist;
  struct c *next;
} Pgm;

typedef struct node
{
  Pgm *pgm;
  char *rstdin; // standard input
  char *rstdout; // standard output if pipelining this will 
  char *rstderr; // error for error messages
  int background; // foreground or background?
} Command;

extern void init(void);
extern int parse(char *, Command *);
extern int nexttoken(char *, char **);
extern int acmd(char *, Pgm **);
extern int isidentifier(char *);
