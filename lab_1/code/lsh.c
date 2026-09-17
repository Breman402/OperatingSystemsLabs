/*
 * Main source file for the lsh shell program.
 *
 * You are free to add functions to this file.
 * If you want to add functions in separate files,
 * you will need to modify CMakeLists.txt to compile
 * your additional files.
 *
 * Add appropriate comments to make your code
 * easier for us to grade.
 *
 * Using assert statements is a good way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>

#include "parse.h"

static void print_cmd(Command *cmd);
void runCMD(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);

int main(void)
{
  signal(SIGINT, SIG_IGN); // Ignore signal SIGINT (Ctrl+C) in the parent process
  for (;;)
  {
    char *line;
    line = readline("> ");

    waitpid(-1, NULL, WNOHANG);

    // This fixes the ctrl + D func.
    if (line == NULL) {
    printf("\n");
    break;
  } else if (line && strcmp(line, "exit") == 0) { // Check if the input line is "exit"
    break;
  }

    // Remove leading and trailing whitespace from the line
    stripwhite(line);

    // If the stripped line is not blank
    if (*line)
    {
      add_history(line);

      Command cmd;
      if (parse(line, &cmd) == 1)
      {
        // Print the parsed command
        print_cmd(&cmd);
        
        // Create a new process for the parsed command and run it?
        runCMD(&cmd);
      }
      else
      {
        printf("Parse ERROR\n");
      }
    }


    

    // Free the input buffer
    free(line);
  }

  return 0;
}

/*
 * Print a Command structure as returned by parse on stdout.
 *
 * Helper function, no need to change. Might be useful to study as inspiration.
 */
static void print_cmd(Command *cmd_list)
{
  printf("------------------------------\n");
  printf("Parse OK\n");
  printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
  printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
  printf("background: %s\n", cmd_list->background ? "true" : "false");
  printf("Pgms:\n");
  print_pgm(cmd_list->pgm);
  printf("------------------------------\n");
}

/* Print a linked list of Pgm structures.
 *
 * Helper function, no need to change. It may be useful to study for inspiration.
 */
static void print_pgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is stored in reverse order, so print
     * it in reverse to restore the original order.
     */
    print_pgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}



// Start a new operating system process to run a command.
void run_simple(Command *cmd) {
  int background = cmd->background;
  pid_t pid = fork();

  if (pid < 0) {
    fprintf(stderr, "Fork failed");
  } else if (pid == 0) {
    // Child process

    if (background == 0) { // if not a background process, die from ctrl + c
      // This setting only applies to this child process! 
      signal(SIGINT, SIG_DFL);
    }
      
    // Look at the programlist, fetch the top one:
    const char* commandToExecute = cmd->pgm->pgmlist[0];
    
    // Look at the programlist, fetch the rest of the arguments:
    char *const *argument = cmd->pgm->pgmlist;
    
    // Execute the command with the provided arguments
    execvp(commandToExecute, argument);
    
  } else {
    // Parent process
    
    if (background == 0) { // if not a background process
      // Wait for the child process to finish or a ctrl + c signal to be recived
      waitpid(pid, NULL, 0);

    } else if (background == 1) {
      // Do not wait for the child process to finish
      printf("Process running in background with PID: %d\n", pid);
    } else {
      // Invalid background value
      fprintf(stderr, "Invalid background value: %d\n", background);
    }
  }
}


void run_piped(Command *cmd) {
  int background = cmd->background;
  Pgm *last  = cmd->pgm;       // last command in the pipeline
  Pgm *first = cmd->pgm->next; // first command in the pipeline

  int fd[2]; //communication channels
  if (pipe(fd) == -1) { perror("pipe"); return; }

  //child 1 (writes in pipe)
  pid_t pid1 = fork();
  
  if (pid1 < 0) {
    fprintf(stderr, "Fork failed");
  }
  else if (pid1 == 0) {
    if (background == 0) {
      signal(SIGINT, SIG_DFL);
    }
    dup2(fd[1], 1);
    close(fd[0]); close(fd[1]);
    execvp(first->pgmlist[0], first->pgmlist);
    exit(1);
  
  }

  //child 2 (reading from pipe)
  pid_t pid2 = fork();
  if (pid2 < 0) {
    fprintf(stderr, "Fork failed");
  }
  else if (pid2 == 0) {
    if (background == 0) signal(SIGINT, SIG_DFL);
    dup2(fd[0], 0);
    close(fd[0]); close(fd[1]);
    execvp(last->pgmlist[0], last->pgmlist);
    exit(1);
  }


    // parent close the pipe
    close(fd[0]);
    close(fd[1]);

    
    
    if (background == 0) {
      // Wait for the child process to finish or a ctrl + c signal to be recived
      waitpid(pid1, NULL, 0);
      waitpid(pid2, NULL, 0);
      
    } else if (background == 1) {
      // Do not wait for the child process to finish
      printf("Process running in background with PID: %d\n", pid1);
    } else {
      // Invalid background value
      fprintf(stderr, "Invalid background value: %d\n", background);
    }
  
}

void runCMD(Command *cmd_list) {
  if (cmd_list->pgm->next == NULL){
    run_simple(cmd_list);
  }else{
    run_piped(cmd_list);
  }
}






/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  size_t i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    memmove(string, string + i, strlen(string + i) + 1);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}
