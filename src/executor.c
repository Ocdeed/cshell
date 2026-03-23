/*
 * cshell/src/executor.c - Command execution engine
 * 
 * RESPONSIBILITY: Execute parsed commands
 * 
 * This is where the magic happens:
 * - fork() creates new processes
 * - execvp() runs programs
 * - pipe() creates IPC channels
 * - dup2() sets up redirections
 * 
 * WHY SEPARATE: Execution logic is independent of parsing.
 * - Can test execution without parsing
 * - Different shells could reuse this logic
 * - Clean separation of concerns
 */

#include "../include/executor.h"
#include "../include/builtins.h"
#include "../include/jobs.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

/*
 * =============================================================================
 * EDUCATIONAL IMPLEMENTATION - Simple execute_command()
 * =============================================================================
 * 
 * This is a simpler version that demonstrates the core concepts.
 * See fork_and_exec() for the full implementation with pipes/redirections.
 */

/*
 * simple_execute() - Fork and execute a command (educational version)
 * 
 * This is the SIMPLEST way to run an external command in C.
 * 
 * Steps:
 * 1. fork() - Create a child process
 * 2. In child: execvp() - Replace child with the command
 * 3. In parent: waitpid() - Wait for child to finish
 * 
 * WHAT HAPPENS IN MEMORY:
 * 
 * BEFORE fork():
 * ┌─────────────────────────────────────────┐
 * │ Shell Process                            │
 * │ ┌─────────────────────────────────────┐ │
 * │ │ Code: main(), printf(), fork()...   │ │
 * │ │ Data: variables, buffers            │ │
 * │ │ Stack: function calls, local vars   │ │
 * │ └─────────────────────────────────────┘ │
 * └─────────────────────────────────────────┘
 * 
 * AFTER fork():
 * ┌───────────────────┐   ┌───────────────────┐
 * │ Parent Process    │   │ Child Process     │
 * │ (shell)          │   │ (exact copy!)     │
 * │ fork() = pid>0   │   │ fork() = 0        │
 * └───────────────────┘   └───────────────────┘
 * 
 * AFTER execvp() in child:
 * ┌───────────────────┐   ┌───────────────────┐
 * │ Parent Process    │   │ Child Process     │
 * │ (shell)          │   │ (NOW 'ls'!)       │
 * │ fork() = pid>0   │   │ execvp() returned!│
 * │ still running     │   │ "command not found"│
 * └───────────────────┘   └───────────────────┘
 * 
 * WHY MEMORY IS COPIED:
 * - fork() uses "copy-on-write"
 * - Initially, parent and child SHARE memory pages
 * - When either writes, a copy is made
 * - This is fast for fork() followed by exec()
 */
void simple_execute(char **args)
{
    pid_t pid;
    int status;
    
    /*
     * STEP 1: fork()
     * 
     * fork() creates a new process by duplicating the current one.
     * 
     * RETURN VALUE:
     * - In PARENT: returns child's PID (positive number, e.g., 12345)
     * - In CHILD:  returns 0
     * - On ERROR:  returns -1 (very rare, usually out of process slots)
     * 
     * WHY IT RETURNS TWICE:
     * - fork() creates the child, then BOTH processes continue
     * - The child starts execution at the SAME LINE where fork() returns
     * - Both run the code AFTER fork(), but with different return values
     * 
     * INTERVIEW QUESTION: What does fork() actually do?
     * - Creates a copy of the entire process
     * - All file descriptors are duplicated (inherited by child)
     * - All memory is copied (copy-on-write optimization)
     * - Child gets its own PID
     */
    pid = fork();
    
    /*
     * ERROR CHECKING - Critical!
     * 
     * fork() can fail in rare cases:
     * - System is out of process slots
     * - User has hit process limit (ulimit -u)
     * - Kernel memory is exhausted
     * 
     * If we don't check, we'd have an invalid PID and chaos ensues.
     */
    if (pid < 0) {
        perror("fork");  /* Print: "fork: Resource temporarily unavailable" */
        return;
    }
    
    /*
     * STEP 2: Child process - execvp()
     * 
     * The child process runs this code because fork() returned 0.
     * 
     * WHAT execvp() DOES:
     * - Searches for the command in PATH directories
     * - Loads the executable into memory
     * - REPLACES the child's code/data with the new program
     * - NEVER RETURNS if successful (child IS now the command!)
     * - RETURNS -1 if command not found or can't execute
     * 
     * WHY execvp() and not execv()?
     * - execv("/bin/ls", ["ls", NULL]) - exact path required
     * - execvp("ls", ["ls", NULL]) - searches PATH directories
     * 
     * INTERVIEW QUESTION: What happens to memory after exec()?
     * - EVERYTHING is replaced!
     * - Code segment: replaced with new program's code
     * - Data segment: replaced with new program's data
     * - Stack: reset for new program
     * - The PID stays the same (same process, different program)
     */
    if (pid == 0) {
        /* 
         * execvp() arguments:
         * - args[0]: command name (used for PATH search)
         * - args: NULL-terminated array of arguments
         * 
         * Example: execvp("ls", ["ls", "-la", NULL])
         */
        execvp(args[0], args);
        
        /*
         * If we reach HERE, execvp() FAILED!
         * 
         * Common reasons for failure:
         * - "ls: command not found" - command doesn't exist
         * - "Permission denied" - no execute permission
         * 
         * We print error and MUST exit (child shouldn't continue shell!)
         * Use _exit() not exit():
         * - exit() does cleanup (atexit handlers, flushing buffers)
         * - _exit() is immediate termination (safer in child)
         */
        fprintf(stderr, "%s: command not found\n", args[0]);
        _exit(127);  /* 127 = command not found (POSIX convention) */
    }
    
    /*
     * STEP 3: Parent process - waitpid()
     * 
     * The parent runs this code because fork() returned child's PID (> 0).
     * 
     * waitpid() suspends the parent until child finishes.
     * 
     * WHY MUST WE WAIT?
     * - If we don't wait, child becomes a ZOMBIE
     * - Zombie = dead process that hasn't been reaped
     * - Zombies occupy process table slots
     * - Too many zombies = "out of processes" error
     * 
     * INTERVIEW QUESTION: What is a zombie process?
     * - A process that has terminated but hasn't been wait()ed on
     * - Still has entry in process table (PID, exit status)
     * - Defunct process - can't be killed (already dead!)
     * 
     * SOLUTION: Always call wait/waitpid after fork!
     */
    
    /*
     * waitpid() arguments:
     * - pid: which process to wait for
     *   * > 0: wait for specific PID
     *   * -1:  wait for any child
     *   * 0:   wait for any child in same process group
     * - status: where to store exit status
     * - options: behavior flags
     *   * 0: block until child exits
     *   * WNOHANG: return immediately if child hasn't exited
     *   * WUNTRACED: also return if child is stopped
     */
    waitpid(pid, &status, 0);
    
    /*
     * Exit status handling (optional - just for completeness)
     * 
     * WIFEXITED(status): did child exit normally?
     * WEXITSTATUS(status): what was the exit code?
     */
}

/*
 * =============================================================================
 * PIPE IMPLEMENTATION - Educational version
 * =============================================================================
 */

/*
 * WHAT IS A PIPE?
 * 
 * A pipe is a one-way data channel implemented in the kernel:
 * - Has a read end (fd[0]) and write end (fd[1])
 * - Data written to fd[1] can be read from fd[0]
 * - Kernel maintains a buffer (typically 4KB-64KB)
 * - If buffer is full, writer blocks; if empty, reader blocks
 * 
 * ┌─────────────────────────────────────────────┐
 * │              KERNEL PIPE BUFFER               │
 * │   [written data bytes] ──────────────▶       │
 * │   ◀───────────── [read data bytes]          │
 * └─────────────────────────────────────────────┘
 *              ↑                    ↑
 *              │                    │
 *         fd[1] (write)        fd[0] (read)
 */

/*
 * execute_piped() - Execute two commands with a pipe between them
 * 
 * This implements: left_args | right_args
 * 
 * Example: execute_piped(["ls", NULL], ["grep", "c", NULL])
 * This runs: ls | grep c
 * 
 * THE COMPLETE FLOW:
 * 
 * ┌─────────────────────────────────────────────────────────────────┐
 * │ STEP 1: Create pipe                                               │
 * │   pipe(pipefd) → pipefd[0]=3 (read), pipefd[1]=4 (write)        │
 * └─────────────────────────────────────────────────────────────────┘
 *                              │
 *                              ▼
 * ┌─────────────────────────────────────────────────────────────────┐
 * │ STEP 2: Fork LEFT process (ls)                                   │
 * │   fork() → returns child PID in parent, 0 in child              │
 * └─────────────────────────────────────────────────────────────────┘
 *                              │
 *              ┌───────────────┴───────────────┐
 *              ▼                               ▼
 * ┌───────────────────────────┐   ┌───────────────────────────┐
 * │ LEFT CHILD (ls)          │   │ PARENT                    │
 * │ pid = 0                  │   │ pid = child PID           │
 * │                          │   │                           │
 * │ dup2(pipefd[1], STDOUT)  │   │ Continue to step 3...     │
 * │ close(pipefd[0])         │   │                           │
 * │ close(pipefd[1])         │   │                           │
 * │ execvp("ls", ["ls",...])│   │                           │
 * └───────────────────────────┘   └───────────────────────────┘
 *              │
 *              ▼
 * ┌─────────────────────────────────────────────────────────────────┐
 * │ STEP 3: Fork RIGHT process (grep)                               │
 * └─────────────────────────────────────────────────────────────────┘
 *                              │
 *              ┌───────────────┴───────────────┐
 *              ▼                               ▼
 * ┌───────────────────────────┐   ┌───────────────────────────┐
 * │ RIGHT CHILD (grep)        │   │ PARENT                    │
 * │ pid = 0                    │   │ pid = child PID           │
 * │                           │   │                           │
 * │ dup2(pipefd[0], STDIN)    │   │ Close pipe ends:          │
 * │ close(pipefd[1])          │   │ close(pipefd[0])          │
 * │ close(pipefd[0])          │   │ close(pipefd[1])          │
 * │ execvp("grep", [...])     │   │                           │
 * └───────────────────────────┘   │ waitpid() both children   │
 *              │                  └───────────────────────────┘
 *              │
 *              ▼
 * ┌─────────────────────────────────────────────────────────────────┐
 * │ DATA FLOW (while processes run)                                  │
 * │                                                                 │
 * │   ls PROCESS ─── writes ──▶ [PIPE BUFFER] ─── reads ─── grep  │
 * │   (stdout=pipefd[1])                    (stdin=pipefd[0])      │
 * └─────────────────────────────────────────────────────────────────┘
 */
void execute_piped(char **left_args, char **right_args)
{
    int pipefd[2];      /* pipefd[0]=read, pipefd[1]=write */
    pid_t left_pid;     /* PID of left process (ls) */
    pid_t right_pid;    /* PID of right process (grep) */
    
    /*
     * STEP 1: Create the pipe
     * 
     * pipe() creates a unidirectional channel:
     * - pipefd[0] = read end (we READ from this)
     * - pipefd[1] = write end (we WRITE to this)
     * 
     * The pipe exists in kernel memory - both processes
     * access the same buffer through their file descriptors.
     */
    if (pipe(pipefd) < 0) {
        perror("pipe");
        return;
    }
    
    /*
     * STEP 2: Fork LEFT process (ls)
     * 
     * This fork creates the producer side of the pipe.
     * The child will:
     * 1. Redirect stdout TO the pipe
     * 2. Run ls
     */
    left_pid = fork();
    
    if (left_pid < 0) {
        /* Fork failed */
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }
    
    if (left_pid == 0) {
        /*
         * LEFT CHILD PROCESS (ls)
         * 
         * dup2() redirects:
         * - Original stdout (fd 1) → pipe write end (pipefd[1])
         * - After this, printf() writes go to the pipe!
         * 
         * FILE DESCRIPTOR TABLE AFTER dup2:
         * ┌────────┬─────────────────────┐
         * │   0    │ stdin               │
         * │   1    │ pipefd[1] (write)  │  ← stdout now points to pipe!
         * │   2    │ stderr              │
         * │   3    │ pipefd[0] (read)   │
         * │   4    │ (closed)            │
         * └────────┴─────────────────────┘
         */
        dup2(pipefd[1], STDOUT_FILENO);
        
        /*
         * CRITICAL: Close all pipe ends we don't use!
         * 
         * Why close pipefd[0] (read end)?
         * - This child only WRITES to the pipe
         * - Not closing causes resource leak
         * - If grep closes its write end, but we still have
         *   an open write end, the read end won't get EOF!
         * 
         * Why close pipefd[1] after dup2?
         * - dup2() already redirected stdout to pipefd[1]
         * - Having an extra reference can cause issues
         * - Best practice: close the original after dup2
         */
        close(pipefd[0]);  /* Don't need to read from pipe */
        close(pipefd[1]);   /* Original fd, now redirected */
        
        /*
         * Execute the left command
         */
        execvp(left_args[0], left_args);
        
        /* If execvp returns, it failed */
        perror(left_args[0]);
        _exit(127);
    }
    
    /*
     * STEP 3: Fork RIGHT process (grep)
     * 
     * This fork creates the consumer side of the pipe.
     * The child will:
     * 1. Redirect stdin FROM the pipe
     * 2. Run grep
     */
    right_pid = fork();
    
    if (right_pid < 0) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }
    
    if (right_pid == 0) {
        /*
         * RIGHT CHILD PROCESS (grep)
         * 
         * dup2() redirects:
         * - Original stdin (fd 0) → pipe read end (pipefd[0])
         * - After this, scanf()/getchar() reads from the pipe!
         * 
         * FILE DESCRIPTOR TABLE AFTER dup2:
         * ┌────────┬─────────────────────┐
         * │   0    │ pipefd[0] (read)   │  ← stdin now points to pipe!
         * │   1    │ stdout              │
         * │   2    │ stderr              │
         * │   3    │ (closed)            │
         * │   4    │ pipefd[1] (write)  │
         * └────────┴─────────────────────┘
         */
        dup2(pipefd[0], STDIN_FILENO);
        
        /*
         * CRITICAL: Close all pipe ends we don't use!
         * 
         * Why close pipefd[1] (write end)?
         * - This child only READS from the pipe
         * - Not closing: grep would never see EOF!
         *   (EOF only comes when ALL write ends are closed)
         */
        close(pipefd[1]);  /* Don't need to write to pipe */
        close(pipefd[0]);  /* Original fd, now redirected */
        
        /*
         * Execute the right command
         */
        execvp(right_args[0], right_args);
        
        /* If execvp returns, it failed */
        perror(right_args[0]);
        _exit(127);
    }
    
    /*
     * STEP 4: Parent closes both ends and waits
     * 
     * The parent created the pipe and forked both children.
     * Now the parent must:
     * 1. Close its references to both pipe ends
     * 2. Wait for both children to finish
     * 
     * IMPORTANT: Parent does NOT use the pipe!
     * The parent just orchestrates the children.
     */
    
    /* Close pipe ends in parent */
    /* Parent doesn't read or write - children do all the work */
    close(pipefd[0]);  /* Close read end - we don't read */
    close(pipefd[1]);  /* Close write end - we don't write */
    
    /*
     * Wait for both children
     * 
     * We use waitpid() to wait for each child:
     * - Left process (ls) finishes first (produces all output)
     * - Right process (grep) then gets EOF and finishes
     * - Or both finish together
     */
    waitpid(left_pid, NULL, 0);   /* Wait for ls */
    waitpid(right_pid, NULL, 0);  /* Wait for grep */
}

/*
 * =============================================================================
 * REDIRECTION - Educational implementation
 * =============================================================================
 */

/*
 * WHAT IS I/O REDIRECTION?
 * 
 * stdin/stdout/stderr are just file descriptors 0/1/2:
 * 
 * ┌────────┬──────────────────┐
 * │   0    │ stdin (keyboard) │
 * │   1    │ stdout (terminal)│
 * │   2    │ stderr (terminal)│
 * └────────┴──────────────────┘
 * 
 * Redirection changes where these point.
 */

/*
 * find_redirection() - Find redirection operator in args
 * 
 * Returns index of >, >>, or <, or -1 if none found
 */
int find_redirection(char **args)
{
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0 ||
            strcmp(args[i], ">>") == 0 ||
            strcmp(args[i], "<") == 0) {
            return i;
        }
    }
    return -1;
}

/*
 * handle_redirection() - Execute with I/O redirection
 * 
 * This is the EDUCATIONAL version. The key insight:
 * 
 * We must SAVE the original stdout/stderr BEFORE redirecting,
 * and RESTORE it AFTER the command runs!
 * 
 * THE BUG: If we don't restore, all printf() calls go to the file!
 */
int handle_redirection(char **args)
{
    int redir_pos = find_redirection(args);
    
    if (redir_pos < 0) {
        simple_execute(args);
        return 0;
    }
    
    char *op = args[redir_pos];
    char *filename = args[redir_pos + 1];
    
    if (filename == NULL) {
        fprintf(stderr, "Expected filename after '%s'\n", op);
        return 1;
    }
    
    int target_fd = STDOUT_FILENO;
    int flags;
    
    if (strcmp(op, ">") == 0) {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        target_fd = STDOUT_FILENO;
    }
    else if (strcmp(op, ">>") == 0) {
        flags = O_WRONLY | O_CREAT | O_APPEND;
        target_fd = STDOUT_FILENO;
    }
    else if (strcmp(op, "<") == 0) {
        flags = O_RDONLY;
        target_fd = STDIN_FILENO;
    }
    else {
        fprintf(stderr, "Unknown operator: %s\n", op);
        return 1;
    }
    
    /* KEY FIX: Save original fd before redirecting */
    int saved_fd = dup(target_fd);
    
    /* Open file */
    int fd = open(filename, flags, 0644);
    if (fd < 0) {
        perror(filename);
        close(saved_fd);
        return 1;
    }
    
    /* Redirect */
    dup2(fd, target_fd);
    close(fd);
    
    /* Strip redirection from args */
    args[redir_pos] = NULL;
    args[redir_pos + 1] = NULL;
    
    /* Execute */
    simple_execute(args);
    
    /* KEY FIX: Restore original fd after command! */
    dup2(saved_fd, target_fd);
    close(saved_fd);
    
    return 0;
}

/*
 * =============================================================================
 * FULL IMPLEMENTATION - fork_and_exec() with pipes and redirections
 * =============================================================================
 */

/*
 * setup_redirections() - Apply I/O redirections to a command
 * 
 * For each redirection in the command:
 * - Open the target file with appropriate flags
 * - Dup2 the file descriptor to stdin/stdout
 * 
 * Flags:
 * - O_RDONLY for input (<)
 * - O_WRONLY|O_CREAT|O_TRUNC for output (>)
 * - O_WRONLY|O_CREAT|O_APPEND for append (>>)
 */
int setup_redirections(command_t *cmd)
{
    int fd;
    int i;
    
    for (i = 0; i < cmd->num_redirs; i++) {
        redir_t *r = &cmd->redirs[i];
        
        switch (r->type) {
            case REDIR_IN:
                fd = open(r->filename, O_RDONLY);
                if (fd < 0) {
                    perror(r->filename);
                    return -1;
                }
                if (dup2(fd, STDIN_FILENO) < 0) {
                    perror("dup2");
                    close(fd);
                    return -1;
                }
                close(fd);
                break;
                
            case REDIR_OUT:
                fd = open(r->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror(r->filename);
                    return -1;
                }
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("dup2");
                    close(fd);
                    return -1;
                }
                close(fd);
                break;
                
            case REDIR_APPEND:
                fd = open(r->filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) {
                    perror(r->filename);
                    return -1;
                }
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("dup2");
                    close(fd);
                    return -1;
                }
                close(fd);
                break;
                
            default:
                break;
        }
    }
    
    return 0;
}

/*
 * fork_and_exec() - Fork and execute a single command
 * 
 * This is the core "spawn a process" function.
 * 
 * Parent's perspective:
 * - Fork returns child's PID (in parent)
 * - Parent can wait for child or continue
 * 
 * Child's perspective:
 * - Fork returns 0
 * - Child sets up redirections
 * - Child calls execvp() to replace itself
 * - If execvp returns, it failed (exec never returns on success)
 */
pid_t fork_and_exec(shell_t *shell, command_t *cmd, int in_fd, int out_fd)
{
    pid_t pid;
    
    /* Fork creates a new process
     * 
     * This is the key system call for running commands.
     * After fork():
     * - Parent gets child's PID (> 0)
     * - Child gets 0
     * 
     * INTERVIEW QUESTION: How does fork() work?
     * - Creates a copy of the current process
     * - Copy-on-write: pages shared until written
     * - Returns twice: once in parent, once in child
     */
    pid = fork();
    
    if (pid < 0) {
        /* Fork failed - very rare, usually system out of processes */
        perror("fork");
        return -1;
    }
    
    if (pid == 0) {
        /* CHILD PROCESS */
        
        /* Set up input redirection from pipe if provided */
        if (in_fd != -1) {
            /* Redirect stdin from the pipe's read end */
            if (dup2(in_fd, STDIN_FILENO) < 0) {
                perror("dup2");
                _exit(1);
            }
            close(in_fd);
        }
        
        /* Set up output redirection to pipe if provided */
        if (out_fd != -1) {
            /* Redirect stdout to the pipe's write end */
            if (dup2(out_fd, STDOUT_FILENO) < 0) {
                perror("dup2");
                _exit(1);
            }
            close(out_fd);
        }
        
        /* Apply any file redirections from the command (<, >, >>) */
        if (setup_redirections(cmd) < 0) {
            _exit(1);
        }
        
        /* Execute the command using execvp()
         * 
         * execvp() searches PATH for the command
         * If found, replaces current process image with the new program
         * Never returns if successful (the process IS now the command)
         * Returns -1 on failure (command not found, no execute permission)
         */
        execvp(cmd->argv[0], cmd->argv);
        
        /* Only reaches here if execvp failed */
        fprintf(stderr, "%s: command not found\n", cmd->argv[0]);
        _exit(127);  /* Standard: command not found exit code */
    }
    
    /* PARENT PROCESS - pid contains child's PID */
    return pid;
}

/*
 * execute_pipeline() - Execute commands connected by pipes
 * 
 * For "cmd1 | cmd2 | cmd3":
 * - Create pipe between cmd1 and cmd2
 * - Fork cmd1, redirect output to pipe
 * - Create pipe between cmd2 and cmd3
 * - Fork cmd2, redirect input from pipe1, output to pipe2
 * - Fork cmd3, redirect input from pipe2
 * - Wait for all children
 */
int execute_pipeline(shell_t *shell, command_t *cmd)
{
    int pipefd[2];
    int in_fd = -1;
    int out_fd = -1;
    command_t *current;
    pid_t last_pid = -1;
    int is_background = 0;
    
    /* Check if background execution (& at end of command) */
    if (cmd->num_redirs == -1) {
        is_background = 1;
        cmd->num_redirs = 0;
    }
    
    /* Build command line string for job tracking */
    char cmdline[MAX_LINE] = {0};
    for (current = cmd; current; current = current->next) {
        int i;
        for (i = 0; i < current->argc; i++) {
            if (i > 0) strcat(cmdline, " ");
            strcat(cmdline, current->argv[i]);
        }
        if (current->next) strcat(cmdline, " | ");
    }
    
    /* Iterate through each command in the pipeline */
    for (current = cmd; current != NULL; current = current->next) {
        /* Create pipe to next command (if there is one) */
        if (current->next) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                return -1;
            }
            out_fd = pipefd[1];  /* Write end to next command */
        } else {
            out_fd = -1;  /* Last command, no pipe */
        }
        
        /* Fork and execute this command */
        /* in_fd is output from previous pipe, or -1 for first command */
        last_pid = fork_and_exec(shell, current, in_fd, out_fd);
        
        /* Close pipe ends in parent (child has its own copies) */
        if (in_fd != -1) close(in_fd);
        if (out_fd != -1) close(out_fd);
        
        /* Set up input for next command (read end of this pipe) */
        if (current->next) {
            in_fd = pipefd[0];  /* Read end for next command */
        }
        
        if (last_pid < 0) {
            return -1;
        }
    }
    
    /* Add job to job list */
    if (is_background) {
        add_job(shell, last_pid, cmdline, 1);
        printf("[%d] %d\n", get_next_job_id(shell) - 1, last_pid);
    } else {
        /* Wait for foreground job */
        int status;
        waitpid(last_pid, &status, 0);
    }
    
    return 0;
}

/*
 * execute_command() - Execute a single command (or pipeline)
 * 
 * Top-level execution function.
 * 
 * Steps:
 * 1. Check if command is a built-in (executes in shell process)
 * 2. If built-in AND has redirections: save fd, redirect, execute, restore
 * 3. If not built-in, execute as pipeline (redirections happen in fork)
 */
int execute_command(shell_t *shell, command_t *cmd)
{
    int saved_stdout = -1;
    int saved_stdin = -1;
    int need_restore = 0;
    
    /* Check if it's a built-in command */
    if (is_builtin(cmd->argv[0])) {
        /* 
         * BUG FIX: Builtins need redirections too!
         * 
         * For builtins with redirections (e.g., "echo hello > file.txt"):
         * 1. Save original stdout/stderr
         * 2. Apply redirections
         * 3. Execute builtin
         * 4. Restore original file descriptors
         * 
         * For builtins WITHOUT redirections, skip this overhead.
         */
        if (cmd->num_redirs > 0) {
            saved_stdout = dup(STDOUT_FILENO);
            saved_stdin = dup(STDIN_FILENO);
            need_restore = 1;
            
            if (setup_redirections(cmd) < 0) {
                if (saved_stdout >= 0) {
                    dup2(saved_stdout, STDOUT_FILENO);
                    close(saved_stdout);
                }
                if (saved_stdin >= 0) {
                    dup2(saved_stdin, STDIN_FILENO);
                    close(saved_stdin);
                }
                return -1;
            }
        }
        
        int result = execute_builtin(shell, cmd);
        
        /* Restore original file descriptors */
        if (need_restore) {
            if (saved_stdout >= 0) {
                dup2(saved_stdout, STDOUT_FILENO);
                close(saved_stdout);
            }
            if (saved_stdin >= 0) {
                dup2(saved_stdin, STDIN_FILENO);
                close(saved_stdin);
            }
        }
        
        return result;
    }
    
    /* Not a built-in - execute as pipeline (redirections happen in fork_and_exec) */
    return execute_pipeline(shell, cmd);
}

/*
 * wait_for_job() - Wait for a specific process
 * 
 * Uses waitpid() which is more flexible than wait():
 * - WNOHANG: return immediately if child hasn't exited
 * - WUNTRACED: report stopped children
 * - WCONTINUED: report continued children
 */
pid_t wait_for_job(pid_t pid, int *status)
{
    return waitpid(pid, status, 0);
}