/*
 * cshell/src/builtins.c - Built-in command implementations
 * 
 * RESPONSIBILITY: Implement commands that run IN the shell process
 * 
 * Built-ins are special because:
 * - They DON'T fork() - they run in the current process
 * - They have direct access to shell state (jobs, env vars)
 * - They can modify shell state (cd changes directory)
 * 
 * WHY SEPARATE: Keeps built-in logic isolated from external commands
 * - Easier to test
 * - Different implementation pattern needed
 * - Clear separation of "shell internal" vs "external program"
 */

#include "../include/builtins.h"
#include "../include/jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

/*
 * =============================================================================
 * EDUCATIONAL IMPLEMENTATION - Simple Built-in Dispatcher
 * =============================================================================
 */

/*
 * WHY BUILT-INS CAN'T BE EXTERNAL PROCESSES:
 * 
 * Example: If "cd" was an external program (/bin/cd):
 * 
 *   Shell (pid=100)                    /bin/cd (pid=101)
 *   cwd: /home/user                    cwd: (inherited /home/user)
 *         │                                   │
 *         │──── fork() ───────────────────────▶│
 *         │      (creates child)                │
 *         │                                    │ chdir("/tmp")
 *         │                                    │ (changes ITS cwd)
 *         │                                    │
 *         │◀──────────── exit() ───────────────│
 *         │
 *   cwd: /home/user  ← UNCHANGED! Shell unaffected!
 * 
 * The shell's working directory NEVER changed!
 * 
 * BUT with built-in cd:
 * 
 *   Shell process (single process)
 *   cwd: /home/user
 *         │
 *         │───── directly calls chdir() ───────▶│
 *         │       (no fork!)                    │
 *         │                                      │
 *   cwd: /tmp  ← SHELL'S cwd CHANGED!
 * 
 * Built-ins run IN the shell process, so they CAN modify shell state.
 */

/*
 * simple_is_builtin() - Check if a command is a built-in (educational)
 * 
 * This is the DISPATCHER - it decides whether to fork() or not.
 * 
 * Input:  cmd = "cd"
 * Output: 1 if built-in, 0 if external command
 * 
 * WHY THIS MATTERS:
 * Before fork(), we check if it's a built-in.
 * If yes: run in shell process (no fork needed)
 * If no:  fork() and run in child process
 */
int simple_is_builtin(const char *cmd)
{
    if (cmd == NULL) return 0;
    
    if (strcmp(cmd, "cd") == 0) return 1;
    if (strcmp(cmd, "pwd") == 0) return 1;
    if (strcmp(cmd, "exit") == 0) return 1;
    if (strcmp(cmd, "echo") == 0) return 1;
    if (strcmp(cmd, "help") == 0) return 1;
    
    return 0;  /* External command - needs fork() */
}

/*
 * simple_run_builtin() - Execute a built-in command (educational)
 * 
 * These run IN THE CURRENT PROCESS (shell itself).
 * No fork() needed - they modify shell state directly.
 * 
 * EXIT CODES EXPLAINED:
 * 
 * Convention:
 * - Return 0 = success
 * - Return non-zero (1, 2, ...) = error occurred
 * 
 * Why this matters:
 * - Shell can check: if (simple_run_builtin() != 0) { error! }
 * - $? variable holds exit status of last command
 * - Piping/combining commands checks exit codes
 * 
 * Examples:
 * - cd /nonexistent → returns 1 (error)
 * - pwd → returns 0 (success)
 * - exit 42 → exits with status 42
  */

/* Forward declarations */
int simple_cd(char **args);
int simple_pwd(char **args);
int simple_exit(char **args);
int simple_echo(char **args);
int simple_help(char **args);

int simple_run_builtin(char **args)
{
    if (strcmp(args[0], "cd") == 0) {
        return simple_cd(args);
    }
    else if (strcmp(args[0], "pwd") == 0) {
        return simple_pwd(args);
    }
    else if (strcmp(args[0], "exit") == 0) {
        return simple_exit(args);
    }
    else if (strcmp(args[0], "echo") == 0) {
        return simple_echo(args);
    }
    else if (strcmp(args[0], "help") == 0) {
        return simple_help(args);
    }
    
    return -1;  /* Should never reach here */
}

/*
 * simple_cd() - Change directory (built-in)
 * 
 * WHY THIS MUST BE BUILT-IN:
 * - chdir() changes the CURRENT PROCESS's working directory
 * - If external: only /bin/cd's cwd would change, not shell's!
 * 
 * System calls used:
 * - getcwd(): Get current working directory
 * - chdir():   Change current working directory
 * - getenv():  Get environment variable (HOME)
 */
int simple_cd(char **args)
{
    char *path;
    char cwd[MAX_LINE];
    
    /*
     * Step 1: Save current directory for cd - (go back)
     * 
     * We need to remember where we ARE before we change.
     * This enables "cd -" to return to previous directory.
     */
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }
    
    /*
     * Step 2: Determine where to go
     * 
     * Cases:
     * - cd          → go to $HOME
     * - cd /path    → go to /path
     * - cd ..       → go to parent directory
     * - cd -        → go to previous directory
     */
    if (args[1] == NULL) {
        /* No argument: go to HOME */
        path = getenv("HOME");
        if (path == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    }
    else if (strcmp(args[1], "-") == 0) {
        /* cd -: go to previous directory */
        /* TODO: Store previous directory in shell state */
        fprintf(stderr, "cd: OLDPWD not set\n");
        return 1;
    }
    else {
        /* Normal path argument */
        path = args[1];
    }
    
    /*
     * Step 3: Change directory
     * 
     * chdir() returns:
     * - 0 on success
     * - -1 on error (errno set)
     */
    if (chdir(path) != 0) {
        perror("cd");  /* Prints: cd: /path: No such file or directory */
        return 1;       /* Signal error to caller */
    }
    
    /*
     * Step 4: Print new directory (like bash does)
     * Optional: helps user confirm where they are
     */
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    }
    
    return 0;  /* Success */
}

/*
 * run_pwd() - Print working directory (built-in)
 * 
 * This COULD be an external command (/bin/pwd exists).
 * But built-in is faster (no fork/exec overhead).
 */
int simple_pwd(char **args)
{
    char cwd[MAX_LINE];
    
    (void)args;  /* Unused parameter */
    
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
        return 0;  /* Success */
    }
    
    perror("pwd");
    return 1;  /* Error */
}

/*
 * run_exit() - Exit the shell (built-in)
 * 
 * WHY THIS MUST BE BUILT-IN:
 * - Must terminate THE SHELL process, not a child!
 * - External command would exit the child, shell keeps running
 */
int simple_exit(char **args)
{
    int status = 0;
    
    /*
     * Exit status:
     * - exit        → status 0
     * - exit 1      → status 1
     * - exit 127    → status 127
     * 
     * Default is 0 (success)
     */
    if (args[1] != NULL) {
        status = atoi(args[1]);
    }
    
    printf("Exiting with status %d\n", status);
    
    /*
     * exit() vs _exit():
     * - exit()  : flushes buffers, calls atexit handlers, THEN terminates
     * - _exit() : immediate termination (used in child after exec fails)
     * 
     * For the shell itself, use exit() for clean shutdown.
     */
    exit(status);
    
    return 0;  /* Never reached */
}

/*
 * run_echo() - Echo arguments (built-in)
 * 
 * This could be /bin/echo, but built-in is faster.
 */
int simple_echo(char **args)
{
    int i;
    
    for (i = 1; args[i] != NULL; i++) {
        /* Handle $VAR expansion */
        if (args[i][0] == '$') {
            char *val = getenv(args[i] + 1);  /* Skip $ */
            if (val) printf("%s", val);
        } else {
            printf("%s", args[i]);
        }
        if (args[i + 1]) printf(" ");
    }
    printf("\n");
    
    return 0;
}

/*
 * run_help() - Show built-in commands
 */
int simple_help(char **args)
{
    (void)args;  /* Unused */
    
    printf("cshell built-in commands:\n");
    printf("  cd     - Change directory\n");
    printf("  pwd    - Print working directory\n");
    printf("  exit   - Exit the shell\n");
    printf("  echo   - Echo arguments\n");
    printf("  help   - Show this help\n");
    
    return 0;
}

/*
 * =============================================================================
 * FULL IMPLEMENTATION - Command table with function pointers
 * =============================================================================
 */

/*
 * Built-in command table
 * 
 * This array defines all built-in commands.
 * Each entry has: name, function pointer, help text.
 * 
 * Adding new built-ins: add entry to this table + implement function
 */
static builtin_t builtins[] = {
    {"cd", builtin_cd, "Change directory"},
    {"pwd", builtin_pwd, "Print working directory"},
    {"exit", builtin_exit, "Exit the shell"},
    {"echo", builtin_echo, "Print arguments"},
    {"help", builtin_help, "Show help information"},
    {"jobs", builtin_jobs, "List background jobs"},
    {"fg", builtin_fg, "Bring job to foreground"},
    {"bg", builtin_bg, "Resume job in background"},
    {"kill", builtin_kill, "Send signal to job"},
    {"set", builtin_set, "Set/show variables"},
    {"export", builtin_export, "Export variable to environment"},
    {"history", builtin_history, "Show command history"},
};

/*
 * is_builtin() - Check if command is a built-in
 * 
 * Iterates through builtins array and compares names.
 * 
 * Returns: 1 if built-in, 0 if not
 */
int is_builtin(const char *cmd)
{
    int i;
    if (!cmd) return 0;
    
    for (i = 0; i < NUM_BUILTINS; i++) {
        if (strcmp(cmd, builtins[i].name) == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * get_builtin() - Get built-in command by name
 * 
 * Returns: Pointer to builtin_t, or NULL if not found
 */
const builtin_t *get_builtin(const char *cmd)
{
    int i;
    if (!cmd) return NULL;
    
    for (i = 0; i < NUM_BUILTINS; i++) {
        if (strcmp(cmd, builtins[i].name) == 0) {
            return &builtins[i];
        }
    }
    return NULL;
}

/*
 * execute_builtin() - Execute a built-in command
 * 
 * Called from executor.c for built-in commands.
 */
int execute_builtin(shell_t *shell, command_t *cmd)
{
    const builtin_t *b = get_builtin(cmd->argv[0]);
    if (b) {
        return b->func(shell, cmd->argv);
    }
    return -1;
}

/*
 * builtin_cd - Change directory
 * 
 * Usage: cd [directory] [-]
 * 
 * - cd: go to home directory
 * - cd -: go to previous directory
 * - cd path: go to specified path
 * 
 * Uses chdir() system call - changes process working directory
 */
int builtin_cd(shell_t *shell, char **argv)
{
    char *path;
    char cwd[MAX_LINE];
    
    /* Get current directory before changing */
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }
    
    /* No arguments - go to home */
    if (argv[1] == NULL) {
        path = getenv("HOME");
        if (path == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    }
    /* "-" - go to previous directory */
    else if (strcmp(argv[1], "-") == 0) {
        path = shell->last_wd;
        if (path == NULL) {
            fprintf(stderr, "cd: OLDPWD not set\n");
            return 1;
        }
        printf("%s\n", path);
    }
    /* Normal path */
    else {
        path = argv[1];
    }
    
    /* Try to change directory */
    if (chdir(path) != 0) {
        perror("cd");
        return 1;
    }
    
    /* Save current directory for "cd -" */
    free(shell->last_wd);
    shell->last_wd = strdup(cwd);
    
    return 0;
}

/*
 * builtin_pwd - Print working directory
 * 
 * Uses getcwd() system call
 */
int builtin_pwd(shell_t *shell, char **argv)
{
    char cwd[MAX_LINE];
    
    (void)argv;  /* Unused */
    
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
        return 0;
    }
    
    perror("pwd");
    return 1;
}

/*
 * builtin_exit - Exit the shell
 * 
 * Usage: exit [status]
 * 
 * Default exit status is 0.
 */
int builtin_exit(shell_t *shell, char **argv)
{
    int status = 0;
    
    (void)shell;
    
    if (argv[1] != NULL) {
        status = atoi(argv[1]);
    }
    
    exit(status);
}

/*
 * builtin_echo - Echo arguments
 * 
 * Usage: echo [args...]
 * 
 * Simple print with space separation.
 * Supports basic $VAR expansion.
 */
int builtin_echo(shell_t *shell, char **argv)
{
    int i;
    
    (void)shell;
    
    for (i = 1; argv[i] != NULL; i++) {
        /* Simple $VAR expansion */
        if (argv[i][0] == '$') {
            char *val = getenv(argv[i] + 1);
            if (val) printf("%s", val);
        } else {
            printf("%s", argv[i]);
        }
        if (argv[i + 1]) printf(" ");
    }
    printf("\n");
    fflush(stdout);  /* CRITICAL: Flush to ensure output goes to redirect */
    
    return 0;
}

/*
 * builtin_help - Show help
 */
int builtin_help(shell_t *shell, char **argv)
{
    int i;
    
    (void)shell;
    (void)argv;
    
    printf("cshell built-in commands:\n");
    for (i = 0; i < NUM_BUILTINS; i++) {
        printf("  %-10s - %s\n", builtins[i].name, builtins[i].help);
    }
    
    return 0;
}

/*
 * builtin_jobs - List background jobs
 */
int builtin_jobs(shell_t *shell, char **argv)
{
    job_t *job;
    char status_str[32];
    
    (void)argv;
    
    for (job = shell->jobs; job != NULL; job = job->next) {
        format_job_status(job, status_str, sizeof(status_str));
        printf("[%d] %s %s\n", job->job_id, status_str, job->cmdline);
    }
    
    return 0;
}

/*
 * builtin_fg - Bring job to foreground
 */
int builtin_fg(shell_t *shell, char **argv)
{
    job_t *job;
    int job_id;
    
    if (argv[1] == NULL) {
        fprintf(stderr, "fg: job number required\n");
        return 1;
    }
    
    job_id = atoi(argv[1] + 1);  /* Skip % prefix */
    job = find_job(shell, job_id);
    
    if (!job) {
        fprintf(stderr, "fg: no such job\n");
        return 1;
    }
    
    /* Send SIGCONT to resume if stopped */
    if (job->status == JOB_STOPPED) {
        kill(job->pid, SIGCONT);
    }
    
    /* Wait for the job */
    int status;
    waitpid(job->pid, &status, 0);
    update_job_status(shell, job->pid, status);
    
    return 0;
}

/*
 * builtin_bg - Resume job in background
 */
int builtin_bg(shell_t *shell, char **argv)
{
    job_t *job;
    int job_id;
    
    if (argv[1] == NULL) {
        fprintf(stderr, "bg: job number required\n");
        return 1;
    }
    
    job_id = atoi(argv[1] + 1);
    job = find_job(shell, job_id);
    
    if (!job) {
        fprintf(stderr, "bg: no such job\n");
        return 1;
    }
    
    /* Send SIGCONT but don't wait */
    if (job->status == JOB_STOPPED) {
        kill(job->pid, SIGCONT);
        job->status = JOB_RUNNING;
    }
    
    return 0;
}

/*
 * builtin_kill - Send signal to job
 */
int builtin_kill(shell_t *shell, char **argv)
{
    pid_t pid;
    int sig = SIGTERM;
    
    if (argv[1] == NULL) {
        fprintf(stderr, "kill: pid or job required\n");
        return 1;
    }
    
    /* Parse signal number if provided */
    if (argv[1][0] == '-') {
        sig = atoi(argv[1] + 1);
        pid = atoi(argv[2]);
    } else {
        pid = atoi(argv[1]);
    }
    
    if (pid <= 0) {
        fprintf(stderr, "kill: invalid pid\n");
        return 1;
    }
    
    return kill(pid, sig);
}

/*
 * builtin_set - Set/show variables
 */
int builtin_set(shell_t *shell, char **argv)
{
    extern char **environ;
    char **env;
    
    (void)shell;
    
    if (argv[1] == NULL) {
        /* Show all environment variables */
        for (env = environ; *env; env++) {
            printf("%s\n", *env);
        }
        return 0;
    }
    
    if (argv[2] == NULL) {
        /* Show specific variable */
        char *val = getenv(argv[1]);
        if (val) printf("%s\n", val);
        return 0;
    }
    
    /* Set variable */
    setenv(argv[1], argv[2], 1);
    return 0;
}

/*
 * builtin_export - Export variable
 */
int builtin_export(shell_t *shell, char **argv)
{
    (void)shell;
    
    if (argv[1] == NULL) {
        fprintf(stderr, "export: variable required\n");
        return 1;
    }
    
    return setenv(argv[1], getenv(argv[1]) ?: "", 1);
}

/*
 * builtin_history - Show command history
 */
int builtin_history(shell_t *shell, char **argv)
{
    int i;
    
    (void)argv;
    
    for (i = 0; i < shell->history_count; i++) {
        printf("%d %s\n", i + 1, shell->history[i]);
    }
    
    return 0;
}