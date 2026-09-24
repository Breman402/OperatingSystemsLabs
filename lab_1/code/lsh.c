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
#include <fcntl.h>
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
static void apply_redirection(const char *rstdin, const char *rstdout);
void cd(char *arg);

// This funciton is created to run every time a signal for a process being done is received
void sigchld_handler();

int main(void)
{
  signal(SIGINT, SIG_IGN);          // Ignore signal SIGINT (Ctrl+C) in the parent process
  signal(SIGCHLD, sigchld_handler); // Set up so that the funciton gets called
  for (;;)
  {
    char *line;
    line = readline("> ");

    // This fixes the ctrl + D func.
    // If the user presses Ctrl+D, readline returns NULL which should be treated a signal to exit the shell.
    if (line == NULL) {
      printf("\n");
      break;
    }

    // Remove leading and trailing whitespace from the line
    stripwhite(line);

    if (line && strcmp(line, "exit") == 0) { // Check if the input line is "exit"
      break;
    }

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

// Run cd command with path arg
void cd(char *arg)
{
  if (arg == NULL) {
    fprintf(stderr, "Missing argument for cd \n"); // Path not given in input
    return;
  }
  if (chdir(arg) == -1) { // Change directory to the given path
    perror("cd");
  }
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



static void apply_redirection(const char *rstdin, const char *rstdout)
{
  if (rstdin != NULL) {
    int fd = open(rstdin, O_RDONLY);
    if (fd < 0) {
      perror(rstdin);
      _exit(1);
    }
    if (dup2(fd, STDIN_FILENO) < 0) {
      perror("dup2 stdin");
      _exit(1);
    }
    close(fd);
  }

  if (rstdout != NULL) {
    int fd = open(rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
      perror(rstdout);
      _exit(1);
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
      perror("dup2 stdout");
      _exit(1);
    }
    close(fd);
  }
}

// Start a new operating system process to run a command.
void run_simple(Command *cmd) {
  int background = cmd->background; // 1 if background, 0 if foreground
  
  // Treat SIGCHLD normally in the child process.
  if (background == 0) {
    signal(SIGCHLD, SIG_DFL);
  }
  
  pid_t pid = fork();

  if (pid < 0) {
    perror("fork");

    if (background == 0) {
      // Restore the custom SIGCHLD handler if this is a foreground process
      signal(SIGCHLD, sigchld_handler);
    }

    return;
  
  } else if (pid == 0) {
    // Child process

    // Child processes should allways treat SIGCHLD the default way (terminate yourself when done)
    signal(SIGCHLD, SIG_DFL);

    // Move background jobs out of the terminal's foreground process group.
    // This makes it so that background jobs don't receive SIGINT when the user presses Ctrl-C.
    if (background && setpgid(0, 0) < 0) {
      perror("setpgid");
      _exit(1);
    }
    // Every child should terminate when it receives SIGINT.
    signal(SIGINT, SIG_DFL);

    apply_redirection(cmd->rstdin, cmd->rstdout);

    // Look at the programlist, fetch the top one:
    const char* commandToExecute = cmd->pgm->pgmlist[0];
    
    // Look at the programlist, fetch the rest of the arguments:
    char *const *argument = cmd->pgm->pgmlist;
    
    // Execute the command with the provided arguments
    execvp(commandToExecute, argument);
    perror(commandToExecute);
    _exit(1);
    
  } else {
    // Parent process
    
    if (background == 0) { // if foregound process
      
      // Wait for the child process to finish
      waitpid(pid, NULL, 0);

      // use the custom SIGCHLD handler to reap any other child processes that may have finished whilst waiting for this one
      signal(SIGCHLD, sigchld_handler);

      // Reap any children that finished while the custom handler was disabled.
      while (waitpid(-1, NULL, WNOHANG) > 0);
      
    } else if (background == 1) {
      // Do not wait for the child process to finish
      printf("Process running in background with PID: %d\n", pid);
    } else {
      // Invalid background value
      fprintf(stderr, "Invalid background value: %d\n", background);
    }
  }
}

// Helper function: recursively executes the pipeline.
void run_pipeline(Pgm *p, const char *rstdin, const char *rstdout) {
    if (p == NULL) return;

    // Base case: This is the FIRST command typed by the user
    // (but the LAST one in the reverse-linked list).
    if (p->next == NULL) {
        apply_redirection(rstdin, rstdout);

        // Check if the command is "cd"
        // if it is we make use of the cd function to change the directory and exit the child process
        if (strcmp(p->pgmlist[0], "cd") == 0) {
          cd(p->pgmlist[1]);
          _exit(0);
        }
        execvp(p->pgmlist[0], p->pgmlist);
        perror("execvp");
        _exit(1);
    }

    // Recursive case: Set up the pipe
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
      _exit(1);
    }

    // Fork for the left side of the pipe (p->next: the previous commands)
    pid_t pid_left = fork();
    if (pid_left < 0) { perror("fork"); _exit(1); }

    if (pid_left == 0) {
        // The left side writes its output to the pipe
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        run_pipeline(p->next, rstdin, NULL); // Recurse to handle earlier commands
    }

    // Fork for the right side of the pipe (p: the current command)
    pid_t pid_right = fork();
    if (pid_right < 0) { perror("fork"); _exit(1); }

    if (pid_right == 0) {
        // The right side reads its input from the pipe
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        close(fd[1]);
        apply_redirection(NULL, rstdout);

        // Check if the command is "cd"
        // if it is we make use of the cd function to change the directory and exit the child process
        if (strcmp(p->pgmlist[0], "cd") == 0) {
          cd(p->pgmlist[1]);
          _exit(0);
        }
        
        execvp(p->pgmlist[0], p->pgmlist);
        perror("execvp");
        _exit(1);
    }

    // Parent wrapper: Close pipes and wait for both sides to finish.
    // This prevents zombies and ensures the shell waits for the ENTIRE pipeline.
    close(fd[0]);
    close(fd[1]);
    waitpid(pid_left, NULL, 0);
    waitpid(pid_right, NULL, 0);

    // Exit this specific recursive step so it propagates up cleanly
    _exit(0);
}

void run_piped(Command *cmd) {
    // Outer fork: Isolates the entire pipeline execution from the main shell process
    int background = cmd->background; // 1 if background, 0 if foreground

    // Treat SIGCHLD normally in the child process.
    if (background == 0) {
        signal(SIGCHLD, SIG_DFL);
    }    
    
    pid_t pid = fork();
    
    if (pid < 0) {
      perror("fork");

      if (background == 0) {
        // Restore the custom SIGCHLD handler if this is a foreground process
        signal(SIGCHLD, sigchld_handler);
      }

      return;
    }
    
    if (pid == 0) {
        // We are the outer child wrapper
    
        // Restore default signal handling for SIGCHLD in the child process
        signal(SIGCHLD, SIG_DFL);
    
        // Pipeline children inherit this wrapper's process group.
        if (background && setpgid(0, 0) < 0) {
            perror("setpgid");
            _exit(1);
        }
        // Restore default Ctrl-C behavior for foreground and background jobs.
        signal(SIGINT, SIG_DFL);
        
        run_pipeline(cmd->pgm, cmd->rstdin, cmd->rstdout);
        _exit(1); // Should only be reached if pgm is NULL
    }

    // Main shell process
    if (background == 0) {
        // Wait for the child process to finish
        waitpid(pid, NULL, 0);

        // Restore the custom SIGCHLD handler
        signal(SIGCHLD, sigchld_handler);

        // Reap any children that finished while the custom handler was disabled.
        while (waitpid(-1, NULL, WNOHANG) > 0);

    } else {
        printf("Process running in background with PID: %d\n", pid);
    }
}

void runCMD(Command *cmd_list) {
// If the command list pgm next is NULL, it means there is only one command to run, so we call run_simple.
  if (cmd_list->pgm->next == NULL && strcmp(cmd_list->pgm->pgmlist[0], "cd") == 0) {
    cd(cmd_list->pgm->pgmlist[1]);
    return;
  }else if (cmd_list->pgm->next == NULL){
      run_simple(cmd_list);
    }else{
      run_piped(cmd_list);
    }
}

void sigchld_handler() {
    // Reap any zombie processes that may have been created by background processes
    // Has to be a while loop because there may be multiple zombie processes to reap.
    while (waitpid(-1, NULL, WNOHANG) > 0);
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
