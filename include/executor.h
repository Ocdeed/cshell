/*
 * cshell/include/executor.h - Executor module header
 * 
 * The executor is responsible for:
 * 1. Creating child processes with fork()
 * 2. Setting up I/O redirections
 * 3. Managing pipes between commands
 * 4. Handling background execution
 * 5. Waiting for foreground processes
 * 
 * This is where the shell's process management happens.
 */

#ifndef CSHELL_EXECUTOR_H
#define CSHELL_EXECUTOR_H

#include "shell.h"

/*
 * execute_command() - Execute a single command (possibly with redirects)
 * 
 * shell: Shell state structure
 * cmd: Parsed command structure
 * Returns: 0 on success, -1 on error
 * 
 * This function:
 * - Forks a child process
 * - Sets up redirections in child
 * - Uses execvp() to run the command
 * - Waits for child in parent (unless backgrounded)
 */
int execute_command(shell_t *shell, command_t *cmd);

/*
 * execute_pipeline() - Execute a chain of commands connected by pipes
 * 
 * shell: Shell state structure  
 * cmd: First command in pipeline (linked list)
 * Returns: 0 on success, -1 on error
 * 
 * For pipeline "cmd1 | cmd2 | cmd3":
 * - Create pipe between cmd1 and cmd2
 * - Create pipe between cmd2 and cmd3
 * - Fork processes for each command
 * - Connect stdout of cmd1 to stdin of cmd2 via pipe
 * - Connect stdout of cmd2 to stdin of cmd3 via pipe
 */
int execute_pipeline(shell_t *shell, command_t *cmd);

/*
 * fork_and_exec() - Fork and execute a single command with I/O
 * 
 * shell: Shell state
 * cmd: Command to execute
 * in_fd: File descriptor to use for stdin (or -1 for none)
 * out_fd: File descriptor to use for stdout (or -1 for none)
 * Returns: Child PID in parent, or -1 on error (never returns in child)
 * 
 * This is the core "spawn a process" function:
 * - Forks the process
 * - Child: sets up redirections, then execvp()
 * - Parent: returns PID for job tracking
 */
pid_t fork_and_exec(shell_t *shell, command_t *cmd, int in_fd, int out_fd);

/*
 * setup_redirections() - Apply redirections for a command
 * 
 * cmd: Command with redirection specifications
 * Returns: 0 on success, -1 on error
 * 
 * Handles:
 * - < file: open file, dup2 to stdin (fd 0)
 * - > file: open (truncate), dup2 to stdout (fd 1)
 * - >> file: open (append), dup2 to stdout (fd 1)
 */
int setup_redirections(command_t *cmd);

/*
 * wait_for_job() - Wait for a foreground process/job
 * 
 * pid: Process ID to wait for
 * status: Pointer to store wait status
 * Returns: PID of exited process, or -1 on error
 * 
 * Uses waitpid() with WNOHANG for background jobs,
 * blocking wait for foreground jobs.
 */
pid_t wait_for_job(pid_t pid, int *status);

#endif /* CSHELL_EXECUTOR_H */