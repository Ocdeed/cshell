/*
 * cshell/include/jobs.h - Job control module header
 * 
 * Job control is the mechanism by which shells manage background
 * and foreground processes. It involves:
 * - Process groups (all processes in a pipeline)
 * - Terminal control (which process group owns the terminal)
 * - Signals (SIGSTOP, SIGCONT, SIGTTIN, SIGTTOU)
 * 
 * This module manages the list of background jobs and their states.
 */

#ifndef CSHELL_JOBS_H
#define CSHELL_JOBS_H

#include "shell.h"

/*
 * init_shell() - Initialize shell state
 * 
 * Sets up initial job list, history, process group.
 * Called once at shell startup.
 */
void init_shell(shell_t *shell);

/*
 * cleanup_shell() - Clean up shell state on exit
 * 
 * Frees all allocated memory, kills remaining jobs.
 * Called before shell exits.
 */
void cleanup_shell(shell_t *shell);

/*
 * add_job() - Add a new background job to the list
 * 
 * shell: Shell state
 * pid: Process ID of the job
 * cmd: Command string (for display)
 * is_bg: Is this a background job?
 * 
 * Creates job_t structure and adds to linked list.
 * Assigns sequential job IDs (1, 2, 3...).
 */
void add_job(shell_t *shell, pid_t pid, const char *cmd, int is_bg);

/*
 * remove_job() - Remove a job from the list
 * 
 * shell: Shell state
 * pid: Process ID to remove
 * 
 * Called when process exits. Frees memory.
 */
void remove_job(shell_t *shell, pid_t pid);

/*
 * find_job() - Find job by job ID or PID
 * 
 * shell: Shell state
 * job_id: Job ID (positive number) or PID (negative, or 0)
 * Returns: Pointer to job_t, or NULL if not found
 * 
 * Usage:
 * - find_job(shell, 1) finds job with ID 1 (%1)
 * - find_job(shell, -pid) finds job with matching PID
 */
job_t *find_job(shell_t *shell, int job_id);

/*
 * update_job_status() - Update job status after waitpid
 * 
 * shell: Shell state
 * pid: Process that changed
 * status: Status returned by waitpid
 * 
 * Checks WIFEXITED, WIFSIGNALED, WIFSTOPPED, WIFCONTINUED
 * and updates job status accordingly.
 */
void update_job_status(shell_t *shell, pid_t pid, int status);

/*
 * get_next_job_id() - Get next available job ID
 * 
 * shell: Shell state
 * Returns: Next job ID number
 * 
 * Simple counter - not reused after job exits.
 * This matches bash behavior.
 */
int get_next_job_id(shell_t *shell);

/*
 * format_job_status() - Format status string for display
 * 
 * job: Job to format
 * buffer: Output buffer
 * size: Buffer size
 * 
 * Returns string like "Running" or "Stopped" or "Done".
 */
void format_job_status(job_t *job, char *buffer, size_t size);

/*
 * reap_zombies() - Reap any terminated background processes
 * 
 * shell: Shell state
 * 
 * Called periodically (or after each builtin) to clean up
 * zombie processes. Uses waitpid with WNOHANG.
 */
void reap_zombies(shell_t *shell);

#endif /* CSHELL_JOBS_H */