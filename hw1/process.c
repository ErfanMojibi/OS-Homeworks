#include "process.h"
#include "shell.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <termios.h>
#include "parse.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

/**
 * gives program path
 */
char *get_program_path(tok_t arg[])
{
  if (arg[0][0] == '/')
  {
    return arg[0];
  }
  else
  {
    char *PATH = (char *)calloc(1024, 1);
    strcpy(PATH, getenv("PATH")); // get path
    tok_t *t = getToks(PATH);
    for (int i = 0; t[i] != NULL; i++)
    {
      char* read_path = (char*)malloc(1024*sizeof(char*));
      strcpy(read_path, t[i]);
      strcat(read_path, "/");
      strcat(read_path, arg[0]);
      if (program_exists(read_path))
      {
        free(PATH);
        return read_path;
      } else {
        free(read_path);
      }
    }
    return NULL;
  }
}

/**
 * Executes the process p.
 * If the shell is in interactive mode and the process is a foreground process,
 * then p should take control of the terminal.
 */
void launch_process(process *p)
{
  pid_t child_pid = fork();
  if (child_pid < 0)
  {
    fprintf(stdin, "error in fork");
    exit(-1);
  } else if (child_pid == 0){
    int out_index = isDirectTok(p->argv, ">");
    int stdout_dup;
    if (out_index != -1)
    {
      fflush(stdout);
      stdout_dup = dup(fileno(stdout));
      int user_file = open(p->argv[out_index + 1], O_CREAT | O_TRUNC | O_WRONLY, 0666);
      dup2(user_file, fileno(stdout));
      close(stdout);
      p->argv[out_index] = NULL;
    }

    // input redirect
    int in_index = isDirectTok(p->argv, "<");
    int stdin_dup;
    if (in_index != -1)
    {
      stdin_dup = dup(fileno(stdin));
      int user_file = open(p->argv[in_index + 1], O_RDONLY | O_CREAT, 0666);
      dup2(user_file, fileno(stdin));

      fflush(stdin);
      close(stdin);
      p->argv[in_index] = NULL;
    }

    char* path = get_program_path(p->argv);
    execv(path, p->argv);
  
    // output redirect reset
    if (out_index != -1)
    {
      dup2(stdout_dup, fileno(stdout));
      close(stdout_dup);
      fflush(stdout);
    }

    // input redirect reset
    if (in_index != -1)
    {
      dup2(stdin_dup, fileno(stdin));
      close(stdin_dup);
      fflush(stdin);
    }
  
  } else {
    int status;
    int ret = waitpid(child_pid, &status, WIFEXITED(status));

  }
}

/* Put a process in the foreground. This function assumes that the shell
 * is in interactive mode. If the cont argument is true, send the process
 * group a SIGCONT signal to wake it up.
 */
void put_process_in_foreground(process *p, int cont)
{
  /** YOUR CODE HERE */
}

/* Put a process in the background. If the cont argument is true, send
 * the process group a SIGCONT signal to wake it up. */
void put_process_in_background(process *p, int cont)
{
  /** YOUR CODE HERE */
}
