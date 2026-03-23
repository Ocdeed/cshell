/*
 * cshell/include/shell.h - Main header file for the shell
 * 
 * This header defines the core data structures and function declarations
 * used throughout the shell. Think of it as the "contract" that ties
 * all modules together.
 */

#ifndef CSHELL_SHELL_H
#define CSHELL_SHELL_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>

/*
 * MAX_LINE: Maximum length of input line we accept
 * This is reasonable for interactive use - shells typically
 * allow 4096-8192 bytes. We use 4096 to match many common shells.
 */
#define MAX_LINE 4096

/*
 * MAX_ARGS: Maximum number of arguments per command
 * Enough for complex commands but prevents runaway cases
 */
#define MAX_ARGS 256

/*
 * MAX_JOBS: Maximum number of background jobs we track
 */
#define MAX_JOBS 64

/*
 * MAX_HISTORY: Number of commands to store in history
 */
#define MAX_HISTORY 100

/*
 * Command type enumeration - tells us what kind of command we have
 * SIMPLE: Regular command like "ls -la"
 * PIPE: Commands connected with | like "ls | grep foo"
 * SEQUENCE: Commands separated by ; like "cd /; ls"
 * REDIRECT: Commands with < > >> operators
 */
typedef enum {
    CMD_SIMPLE,
    CMD_PIPE,
    CMD_SEQUENCE,
    CMD_REDIRECT
} cmd_type_t;

/*
 * Redirection type enumeration
 * REDIR_IN: < input redirection
 * REDIR_OUT: > output redirection (truncate)
 * REDIR_APPEND: >> output redirection (append)
 */
typedef enum {
    REDIR_NONE,
    REDIR_IN,
    REDIR_OUT,
    REDIR_APPEND
} redir_type_t;

/*
 * Job status enumeration - tracks background job state
 * JOB_RUNNING: Process is running
 * JOB_STOPPED: Process received SIGTSTP (Ctrl+Z)
 * JOB_DONE: Process completed (for display before cleanup)
 */
typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} job_status_t;

/*
 * Redirection descriptor structure
 * Holds information about a single redirection operation
 */
typedef struct {
    redir_type_t type;      /* What kind of redirection */
    char *filename;         /* The file to read/write */
    int fd;                 /* Which file descriptor (0=stdin, 1=stdout) */
} redir_t;

/*
 * Command structure - represents a single command with its arguments
 * and potential redirections
 */
typedef struct command {
    char *argv[MAX_ARGS];   /* Argument vector (including command name) */
    int argc;               /* Number of arguments */
    redir_t *redirs;        /* Array of redirections */
    int num_redirs;         /* Number of redirections */
    struct command *next;   /* Next command in pipeline (for pipes) */
} command_t;

/*
 * Job structure - represents a background job
 */
typedef struct job {
    int job_id;             /* Internal job number (1, 2, 3...) */
    pid_t pid;              /* Process ID of the job */
    job_status_t status;    /* Current status */
    char *cmdline;          /* Full command line for display */
    int is_background;      /* Was it started with & ? */
    struct job *next;       /* Next job in list */
} job_t;

/*
 * Shell state structure - holds global shell state
 * Using a single struct avoids global variables (better for testing)
 */
typedef struct shell {
    job_t *jobs;            /* List of background jobs */
    char *history[MAX_HISTORY];  /* Command history */
    int history_count;     /* Number of commands in history */
    char *last_wd;         /* Previous working directory for cd - */
    int shell_pgid;        /* Process group ID of the shell */
    int interactive;        /* Are we running interactively? */
} shell_t;

/*
 * Function declarations for built-in commands
 */
int builtin_cd(shell_t *shell, char **argv);
int builtin_exit(shell_t *shell, char **argv);
int builtin_pwd(shell_t *shell, char **argv);
int builtin_echo(shell_t *shell, char **argv);
int builtin_jobs(shell_t *shell, char **argv);
int builtin_fg(shell_t *shell, char **argv);
int builtin_bg(shell_t *shell, char **argv);
int builtin_kill(shell_t *shell, char **argv);
int builtin_help(shell_t *shell, char **argv);

/*
 * Check if a command is a built-in (returns index or -1)
 * We'll implement this in builtins.c
 */
int is_builtin(const char *cmd);

/*
 * Parser functions from parser.c
 */
command_t *parse(const char *input);
void free_command(command_t *cmd);
void free_pipeline(command_t *head);

/*
 * Executor functions from executor.c
 */
int execute_command(shell_t *shell, command_t *cmd);
int execute_pipeline(shell_t *shell, command_t *cmd);
pid_t fork_and_exec(shell_t *shell, command_t *cmd, int in_fd, int out_fd);
int execute_builtin(shell_t *shell, command_t *cmd);
int setup_redirections(command_t *cmd);

/*
 * Job control functions from jobs.c
 */
void init_shell(shell_t *shell);
void cleanup_shell(shell_t *shell);
void add_job(shell_t *shell, pid_t pid, const char *cmd, int is_bg);
void remove_job(shell_t *shell, pid_t pid);
job_t *find_job(shell_t *shell, int job_id);
void update_job_status(shell_t *shell, pid_t pid, int status);

/*
 * Signal handling functions
 */
void setup_signals(void);
void ignore_signals(void);
void restore_default_signals(void);

/*
 * History functions
 */
void add_to_history(shell_t *shell, const char *cmd);
char *get_history_entry(shell_t *shell, int index);

#endif /* CSHELL_SHELL_H */