/*
 * cshell/src/jobs.c - Job control implementation
 * 
 * RESPONSIBILITY: Manage background jobs and process state
 * 
 * JOB CONTROL is a Unix concept where:
 * - Shell can run jobs in background (don't block terminal)
 * - Shell can bring jobs to foreground (block terminal)
 * - Ctrl+Z stops foreground job (SIGTSTP)
 * - Ctrl+C interrupts foreground job (SIGINT)
 * 
 * KEY DATA STRUCTURES:
 * - job_t: represents one background job
 * - shell_t.jobs: linked list of all jobs
 * 
 * WHY SEPARATE: Job management is distinct from execution
 * - Complex state tracking
 * - Independent of how commands are run
 * - Makes code testable and maintainable
 */

#include "../include/jobs.h"
#include "../include/shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

/*
 * init_shell() - Initialize shell state
 * 
 * Sets up:
 * - Empty job list
 * - Empty history
 * - Process group for job control
 * 
 * Called once at startup.
 */
void init_shell(shell_t *shell)
{
    /* Initialize job list to empty */
    shell->jobs = NULL;
    
    /* Initialize history to empty */
    shell->history_count = 0;
    memset(shell->history, 0, sizeof(shell->history));
    
    /* Initialize last working directory */
    shell->last_wd = NULL;
    
    /* Get our own process group ID
     * 
     * INTERVIEW QUESTION: What is a process group?
     * - Collection of processes that can be managed together
     * - All processes in a pipeline belong to same process group
     * - Process group ID = leader's PID
     * 
     * This is important for job control signals.
     */
    shell->shell_pgid = getpid();
    
    /* Determine if running interactively
     * 
     * A shell is interactive if:
     * - stdin is a terminal (isatty() returns true)
     * - stdout is a terminal
     * 
     * This affects signal handling and job control behavior.
     */
    shell->interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    
    /* If interactive, put ourselves in our own process group
     * 
     * setpgid(pid, pgid) sets process group:
     * - If pid == 0, use current process
     * - If pgid == 0, pgid = pid (become process group leader)
     * 
     * We call this twice (for parent and child) to ensure
     * the child is in the right group before exec.
     */
    if (shell->interactive) {
        if (setpgid(shell->shell_pgid, shell->shell_pgid) < 0) {
            perror("setpgid");
        }
        
        /* Take control of the terminal
         * 
         * tcsetpgrp() makes our process group the foreground group.
         * Only the foreground group can read from the terminal.
         * Background groups get SIGTTIN if they try to read.
         */
        tcsetpgrp(STDIN_FILENO, shell->shell_pgid);
    }
}

/*
 * cleanup_shell() - Clean up before exit
 * 
 * Frees all allocated memory.
 * Optionally kills remaining background jobs.
 */
void cleanup_shell(shell_t *shell)
{
    job_t *job, *next;
    
    /* Free all jobs */
    for (job = shell->jobs; job != NULL; job = next) {
        next = job->next;
        free(job->cmdline);
        free(job);
    }
    
    /* Free history */
    int i;
    for (i = 0; i < shell->history_count; i++) {
        free(shell->history[i]);
    }
    
    /* Free last working directory */
    free(shell->last_wd);
}

/*
 * get_next_job_id() - Get next sequential job ID
 */
int get_next_job_id(shell_t *shell)
{
    int max_id = 0;
    job_t *job;
    
    for (job = shell->jobs; job != NULL; job = job->next) {
        if (job->job_id > max_id) {
            max_id = job->job_id;
        }
    }
    
    return max_id + 1;
}

/*
 * add_job() - Add a new job to the job list
 * 
 * Creates job_t structure and adds to linked list.
 * 
 * IMPORTANT: Job IDs are sequential (1, 2, 3...) not reused.
 * This matches bash behavior.
 */
void add_job(shell_t *shell, pid_t pid, const char *cmd, int is_bg)
{
    job_t *job = malloc(sizeof(job_t));
    if (!job) return;
    
    job->pid = pid;
    job->status = JOB_RUNNING;
    job->cmdline = strdup(cmd);
    job->is_background = is_bg;
    job->job_id = get_next_job_id(shell);
    job->next = shell->jobs;
    shell->jobs = job;
}

/*
 * remove_job() - Remove a job from the list
 */
void remove_job(shell_t *shell, pid_t pid)
{
    job_t **ptr;
    
    for (ptr = &shell->jobs; *ptr; ptr = &(*ptr)->next) {
        if ((*ptr)->pid == pid) {
            job_t *job = *ptr;
            *ptr = job->next;
            free(job->cmdline);
            free(job);
            return;
        }
    }
}

/*
 * find_job() - Find job by job ID or PID
 * 
 * Usage:
 * - job_id > 0: find by job ID (%1, %2, etc.)
 * - job_id < 0: find by PID
 */
job_t *find_job(shell_t *shell, int job_id)
{
    job_t *job;
    
    if (job_id > 0) {
        /* Search by job ID */
        for (job = shell->jobs; job; job = job->next) {
            if (job->job_id == job_id) {
                return job;
            }
        }
    } else {
        /* Search by PID (negative job_id) */
        pid_t pid = -job_id;
        for (job = shell->jobs; job; job = job->next) {
            if (job->pid == pid) {
                return job;
            }
        }
    }
    
    return NULL;
}

/*
 * update_job_status() - Update job status after wait
 * 
 * Interprets wait status using WIF* macros:
 * - WIFEXITED: process exited normally
 * - WIFSIGNALED: process died from signal
 * - WIFSTOPPED: process was stopped (Ctrl+Z)
 * - WIFCONTINUED: process resumed (bg command)
 */
void update_job_status(shell_t *shell, pid_t pid, int status)
{
    job_t *job;
    
    for (job = shell->jobs; job; job = job->next) {
        if (job->pid == pid) {
            if (WIFEXITED(status)) {
                job->status = JOB_DONE;
            } else if (WIFSIGNALED(status)) {
                job->status = JOB_DONE;
            } else if (WIFSTOPPED(status)) {
                job->status = JOB_STOPPED;
            } else if (WIFCONTINUED(status)) {
                job->status = JOB_RUNNING;
            }
            return;
        }
    }
}

/*
 * format_job_status() - Format status string for job display
 */
void format_job_status(job_t *job, char *buffer, size_t size)
{
    switch (job->status) {
        case JOB_RUNNING:
            snprintf(buffer, size, "Running");
            break;
        case JOB_STOPPED:
            snprintf(buffer, size, "Stopped");
            break;
        case JOB_DONE:
            snprintf(buffer, size, "Done");
            break;
    }
}

/*
 * reap_zombies() - Clean up terminated background processes
 * 
 * Uses waitpid() with WNOHANG to check for dead children
 * without blocking.
 * 
 * This must be called periodically to prevent zombie processes.
 * 
 * INTERVIEW QUESTION: What is a zombie process?
 * - A process that has terminated but hasn't been wait()ed on
 * - Still occupies entry in process table
 * - "Defunct" process
 * - Happens when parent never calls wait()
 * 
 * SOLUTION: Call waitpid() with WNOHANG to reap zombies.
 */
void reap_zombies(shell_t *shell)
{
    pid_t pid;
    int status;
    
    /* Loop until no more zombies to reap */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /* Update job status */
        update_job_status(shell, pid, status);
        
        /* Remove from job list */
        remove_job(shell, pid);
    }
}

/*
 * add_to_history() - Add command to history
 * 
 * Stores command in circular buffer of MAX_HISTORY entries.
 */
void add_to_history(shell_t *shell, const char *cmd)
{
    /* If history is full, free oldest entry */
    if (shell->history_count >= MAX_HISTORY) {
        free(shell->history[0]);
        
        /* Shift everything */
        int i;
        for (i = 0; i < MAX_HISTORY - 1; i++) {
            shell->history[i] = shell->history[i + 1];
        }
        shell->history_count = MAX_HISTORY - 1;
    }
    
    shell->history[shell->history_count++] = strdup(cmd);
}

/*
 * get_history_entry() - Get command from history by index
 * 
 * Returns: Command string, or NULL if index out of range
 * 
 * Note: Index is 1-based (history command shows 1, 2, 3...)
 */
char *get_history_entry(shell_t *shell, int index)
{
    if (index < 1 || index > shell->history_count) {
        return NULL;
    }
    
    return shell->history[index - 1];
}

/*
 * setup_signals() - Set up signal handlers
 * 
 * For interactive shell:
 * - SIGINT (Ctrl+C): interrupt current foreground job
 * - SIGTSTP (Ctrl+Z): stop current foreground job
 * - SIGQUIT (Ctrl+\): core dump (default ignore)
 * - SIGCHLD: child status change (default ignore)
 * 
 * IMPORTANT: Signals must be handled properly:
 * - Parent must handle SIGCHLD to reap zombies
 * - Foreground process group gets the signals
 */
void setup_signals(void)
{
    struct sigaction sa;
    
    /* Ignore SIGQUIT (Ctrl+\) - prevents core dumps */
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGQUIT, &sa, NULL);
    
    /* Default SIGCHLD handling - allows wait() to work
     * Some systems need explicit handler for reliable behavior
     */
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
}

/*
 * ignore_signals() - Ignore certain signals (for child processes)
 */
void ignore_signals(void)
{
    struct sigaction sa;
    
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);   /* Ctrl+C */
    sigaction(SIGTSTP, &sa, NULL);  /* Ctrl+Z */
    sigaction(SIGQUIT, &sa, NULL);  /* Ctrl+\ */
}

/*
 * restore_default_signals() - Restore default signal handlers
 */
void restore_default_signals(void)
{
    struct sigaction sa;
    
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
}