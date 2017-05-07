#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

/* Appends each token acquired from tokens_get_token to an array */
void append(char ***pointer, int *s, char *elm);

// void free_memory(char ***pointer, size_t s);

/* Get the file descriptor of FILE based on TO_FILE, i.e. whether to read or write */
int get_file_des(char *file, int to_file);

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
	{cmd_pwd, "pwd", "print the current working directory"},
	{cmd_cd, "cd", "change directory"},
	{cmd_wait, "wait", "wait for process to complete"}	
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

int cmd_pwd(unused struct tokens *tokens) {
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		perror("cwd() error");
		return errno;
	}
	fprintf(stdout, "%s\n", cwd);
	return 1;
}

int cmd_cd(struct tokens *tokens) {
	char *path;
	if (tokens_get_token(tokens, 1) == NULL)
		path = "/home";
  else
  	path = tokens_get_token(tokens, 1); 
 	if (chdir(path) == 0)
 		return 1;
		//printf("%s\n", path);
 	else
		perror("chdir() error");
 		return errno;
}

int cmd_wait(struct tokens *tokens) {
	int status;
	while (wait(&status) != -1)
		continue;
	return 1;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int main(unused int argc, unused char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;
	
	/* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));
		size_t args_size = 0;
		char *arg;
		char **args = (char **) malloc(sizeof(char *));
 	  char *tok;
		char path[1024];
		char *PATH = getenv("PATH");
		char *filename;
		int to_file = -1;  
		bool background = false;
		int i = 0;

		if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
			
			// get the program path and arguments needed to run the program
			// and store them in ARGS, i.e. ARGS = <prog_name> <arg1> <arg2> ...
			while ((arg = tokens_get_token(tokens, i)) != NULL) {
				if (*arg == '>') {
					filename = tokens_get_token(tokens, ++i);
					to_file = 1;
				}
				else if (*arg == '<') {
					filename = tokens_get_token(tokens, ++i);
					to_file = 0;
				}
				else if (*arg == '&'){
					background = true;
					i++;
				}
				else {
					append(&args, &i, arg);
					args_size = i;
				}
			}
			
			int new_fd;
			if (filename && to_file != -1)
				new_fd = get_file_des(filename, to_file);

			args = (char **) realloc(args, sizeof(char *) * (args_size+1));
			args[args_size] = NULL;
			args_size += 1;
			
			char *is_full = strrchr(args[0], '/');

			if (is_full == NULL) {
				tok = strtok(PATH, ":");
				while (tok != NULL) {
					strcpy(path, tok);
					strcat(path, "/");
					strcat(path, args[0]); 
					if (access(path, F_OK) != -1) {
						args[0] = path;
						break;
					}
					tok = strtok(NULL, ":");
				}
			}
		
			pid_t pid;
			int status;
		
			if ((pid = fork()) < 0) {
				perror("fork() error.");
				exit(1);
			}

			if (pid == 0) {
				signal(SIGINT, SIG_DFL);
				signal(SIGTTIN, SIG_IGN);
				signal(SIGTTOU, SIG_IGN);
				if (!background) {
					tcsetpgrp(0, getpgid(0));
				}
				if (to_file == 1)
					dup2(new_fd, 1);
				else if (to_file == 0)
					dup2(new_fd, 0);
				execv(args[0], args);
			}
			else {
				signal(SIGINT, SIG_IGN);
				signal(SIGTTIN, SIG_IGN);
				signal(SIGTTOU, SIG_IGN);
				setpgid(pid, pid);
				if (!background) {
					waitpid(pid, &status, 0);
					tcsetpgrp(0, shell_pgid);
				}
			}          	
		}
		
    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
			fprintf(stdout, "%d: ", ++line_num);
    
		/* Clean up memory */
    tokens_destroy(tokens);
		//free_memory(&args, args_size);
	}
  return 0;
}

void append(char ***pointer, int *s, char *elm) {
	*pointer = (char **) realloc(*pointer, sizeof(char *) * (*s + 1));
	(*pointer)[*s] = elm;
	*s += 1;
}
/*
void free_memory(char ***pointer, size_t s) {
	for (int i = 0; i < s; i++) {
		free((*pointer)[i]);
	}
}
*/
int get_file_des(char* file, int to_file) {
	int fd = 0;
	if (to_file == 1) {
		if ((fd = open(file, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR)) < 0) {
			perror("open() error");
			exit(1);
		}
	}
	else if (to_file == 0) {
		if ((fd = open(file, O_RDONLY, S_IRUSR|S_IWUSR)) < 0) {
			perror("open() error");
			exit(1);
		}
	}
	return fd;
}
