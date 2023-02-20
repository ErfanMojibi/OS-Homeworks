#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

int cmd_quit(tok_t arg[])
{
  printf("Bye\n");
  exit(0);
  return 1;
}

int cmd_help(tok_t arg[]);
int pwd();
int cd(tok_t arg[]);
int execute_program_with_absolute_path(tok_t arg[]);
int execute_program(tok_t arg[]);
int program_exists(char *path);
/* Command Lookup table */
typedef int cmd_fun_t(tok_t args[]); /* cmd functions take token array and return int */
typedef struct fun_desc
{
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_quit, "quit", "quit the command shell"},
    {pwd, "pwd", "print working directory"},
    {cd, "cd", "change working directory"}};

int cmd_help(tok_t arg[])
{
  int i;
  for (i = 0; i < (sizeof(cmd_table) / sizeof(fun_desc_t)); i++)
  {
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  }
  return 1;
}

int lookup(char cmd[])
{
  int i;
  for (i = 0; i < (sizeof(cmd_table) / sizeof(fun_desc_t)); i++)
  {
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  }
  return -1;
}

void init_shell()
{
  /* Check if we are running interactively */
  shell_terminal = STDIN_FILENO;

  /** Note that we cannot take control of the terminal if the shell
      is not interactive */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive)
  {

    /* force into foreground */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    shell_pgid = getpid();
    /* Put shell in its own process group */
    if (setpgid(shell_pgid, shell_pgid) < 0)
    {
      perror("Couldn't put the shell in its own process group");
      exit(1);
    }

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);
    tcgetattr(shell_terminal, &shell_tmodes);
  }
  /** YOUR CODE HERE */
}

/**
 * Add a process to our process list
 */
void add_process(process *p)
{
  /** YOUR CODE HERE */
}

/**
 * Creates a process given the inputString from stdin
 */
process *create_process(char *inputString)
{
  /** YOUR CODE HERE */
  return NULL;
}

int pwd()
{
  // printf("%s", get_current_dir_name());
  char path[500]; // TODO size
  if (getcwd(path, sizeof(path)) != NULL)
  {
    fprintf(stdout, "%s\n", path);
  }
  else
  {
    fprintf(stdout, "error occured");
  }
  return 1;
}

int cd(tok_t arg[])
{
  if (chdir(arg[0]) != 0)
  {
    fprintf(stdout, "error occured in changing directory\n");
  }
  return 1;
}
int program_exists(char *path)
{
  if (!access(path, F_OK))
  {
    return 1;
  }
  return 0;
}
int execute_program(tok_t arg[])
{
  if (arg[0][0] == '/')
  { // check for absolute path programs
    execute_program_with_absolute_path(arg);
  }
  else
  {
    char *sep = ":";
    char *PATH = (char *)calloc(1024, 1);
    strcpy(PATH, getenv("PATH")); // get path
    int flag = 0;
    if (PATH)
    {
      char *token = strtok(PATH, sep);
      while (token != NULL)
      {
        char read_path[500]; // TODO size

        // concat program name to path
        strcpy(read_path, token);
        strcat(read_path, "/");
        strcat(read_path, arg[0]);

        // execute if program exists
        if (program_exists(read_path))
        {
          arg[0] = read_path;
          execute_program_with_absolute_path(arg);
          // free(PATH);
          flag = 1;
          break;
        }
        else
        {
          token = strtok(NULL, sep);
        }
      }
    }
    else
    {
      fprintf(stdout, "error in getting path\n");
    }
    if (!flag)
      fprintf(stdout, "program didn't exist\n");

    return 1;
  }
}
int execute_program_with_absolute_path(tok_t arg[])
{
  int child_pid = fork();
  if (child_pid == 0)
  {
    // output redirect
    int out_index = isDirectTok(arg, ">");
    int stdout_dup;
    if (out_index != -1)
    {
      fflush(stdout);
      stdout_dup = dup(fileno(stdout));
      int user_file = open(arg[out_index + 1], O_CREAT | O_TRUNC | O_WRONLY, 0666);
      dup2(user_file, fileno(stdout));
      close(stdout);
      arg[out_index] = NULL;
    }

    // input redirect
    int in_index = isDirectTok(arg, "<");
    int stdin_dup;
    if (in_index != -1)
    {
      stdin_dup = dup(fileno(stdin));
      int user_file = open(arg[in_index + 1], O_RDONLY | O_CREAT, 0666);
      dup2(user_file, fileno(stdin));

      fflush(stdin);
      close(stdin);
      arg[in_index] = NULL;
    }

    execv(arg[0], arg);

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
  }
  else
  {
    int status;
    waitpid(child_pid, &status, WUNTRACED | WCONTINUED);
    return 1;
  }
  return 0;
}

int shell(int argc, char *argv[])
{
  char *s = malloc(INPUT_STRING_SIZE + 1); /* user input string */
  tok_t *t;                                /* tokens parsed from input */
  int lineNum = 0;
  int fundex = -1;
  pid_t pid = getpid();   /* get current processes PID */
  pid_t ppid = getppid(); /* get parents PID */
  pid_t cpid, tcpid, cpgid;

  init_shell();

  // printf("%s running as PID %d under %d\n",argv[0],pid,ppid);

  lineNum = 0;
  // fprintf(stdout, "%d: ", lineNum);
  while ((s = freadln(stdin)))
  {
    t = getToks(s);        /* break the line into tokens */
    fundex = lookup(t[0]); /* Is first token a shell literal */

    if (fundex >= 0)
      cmd_table[fundex].fun(&t[1]);
    else
    {
      execute_program(t);
    }

    // fprintf(stdout, "%d: ", lineNum);
  }
  return 0;
}
