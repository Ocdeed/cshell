/*
 * cshell/include/builtins.h - Built-in commands header
 * 
 * Built-in commands are commands that the shell executes directly
 * WITHOUT forking a new process. Examples: cd, pwd, exit.
 * 
 * This separation is important because:
 * - Built-ins need access to shell state (cwd, env vars, jobs)
 * - They run in the same process as the shell
 * - They can modify shell state directly
 */

#ifndef CSHELL_BUILTINS_H
#define CSHELL_BUILTINS_H

#include "shell.h"

/*
 * =============================================================================
 * EDUCATIONAL IMPLEMENTATION - Simple Built-in Dispatcher
 * =============================================================================
 */

/*
 * simple_is_builtin() - Check if command is built-in (educational)
 * 
 * This is the DISPATCHER - decides whether to fork() or not.
 * 
 * Returns: 1 if built-in, 0 if external command
 */
int simple_is_builtin(const char *cmd);

/*
 * simple_run_builtin() - Execute a built-in command (educational)
 * 
 * These run IN THE CURRENT PROCESS (shell itself).
 * No fork() needed - they modify shell state directly.
 * 
 * Returns: 0 on success, non-zero on error
 */
int simple_run_builtin(char **args);

/*
 * =============================================================================
 * FULL IMPLEMENTATION - Command table with function pointers
 * =============================================================================
 */

/*
 * Structure defining a built-in command
 */
typedef struct {
    const char *name;           /* Command name (e.g., "cd") */
    int (*func)(shell_t *, char **); /* Function pointer */
    const char *help;           /* Help text for builtin */
} builtin_t;

/*
 * is_builtin() - Check if a command is a built-in
 * 
 * cmd: Command name to check
 * Returns: 1 if built-in, 0 if external command
 * 
 * Implementation iterates through builtin table and matches name.
 */
int is_builtin(const char *cmd);

/*
 * get_builtin() - Get built-in command by name
 * 
 * cmd: Command name
 * Returns: Pointer to builtin_t, or NULL if not found
 */
const builtin_t *get_builtin(const char *cmd);

/*
 * Built-in command implementations
 */

/*
 * builtin_cd - Change directory
 * Usage: cd [dir] [-]
 * 
 * - No args: cd $HOME
 * - - : cd to previous directory (using last_wd)
 * - Otherwise: cd to specified directory
 * 
 * Uses chdir() system call to change process working directory.
 * Updates last_wd before changing.
 */
int builtin_cd(shell_t *shell, char **argv);

/*
 * builtin_exit - Exit the shell
 * Usage: exit [n]
 * 
 * Exits with optional status code (default 0).
 * Cleans up jobs and history before exiting.
 */
int builtin_exit(shell_t *shell, char **argv);

/*
 * builtin_pwd - Print working directory
 * Usage: pwd
 * 
 * Uses getcwd() system call to get current directory.
 * Prints to stdout.
 */
int builtin_pwd(shell_t *shell, char **argv);

/*
 * builtin_echo - Echo arguments to stdout
 * Usage: echo [args...]
 * 
 * Simple argument printing with space separation.
 * Handles $VAR expansion.
 */
int builtin_echo(shell_t *shell, char **argv);

/*
 * builtin_jobs - List background jobs
 * Usage: jobs
 * 
 * Iterates through job list and prints:
 * [job_id] [status] [command]
 */
int builtin_jobs(shell_t *shell, char **argv);

/*
 * builtin_fg - Bring job to foreground
 * Usage: fg [%job_id]
 * 
 * Sends SIGCONT to process, waits for it.
 * Job becomes the foreground job.
 */
int builtin_fg(shell_t *shell, char **argv);

/*
 * builtin_bg - Resume job in background
 * Usage: bg [%job_id]
 * 
 * Sends SIGCONT to process, doesn't wait.
 * Job continues running in background.
 */
int builtin_bg(shell_t *shell, char **argv);

/*
 * builtin_kill - Send signal to job/process
 * Usage: kill [%job_id | pid] [signal]
 * 
 * Default signal is SIGTERM (15).
 * Can send any signal to process.
 */
int builtin_kill(shell_t *shell, char **argv);

/*
 * builtin_help - Show help information
 * Usage: help
 * 
 * Lists all available built-in commands.
 */
int builtin_help(shell_t *shell, char **argv);

/*
 * builtin_set - Set/show shell variables
 * Usage: set [var [value]]
 * 
 * No args: show all environment variables
 * One arg: show value of variable
 * Two args: set variable to value
 */
int builtin_set(shell_t *shell, char **argv);

/*
 * builtin_export - Export variable to environment
 * Usage: export VAR[=value]
 * 
 * Adds variable to environment for child processes.
 */
int builtin_export(shell_t *shell, char **argv);

/*
 * builtin_history - Show command history
 * Usage: history [-c]
 * 
 * Lists previous commands.
 * -c clears history.
 */
int builtin_history(shell_t *shell, char **argv);

/*
 * Number of built-in commands - for iteration
 */
#define NUM_BUILTINS 12

#endif /* CSHELL_BUILTINS_H */