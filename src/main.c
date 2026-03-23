/*
 * cshell/src/main.c - Main entry point and REPL loop
 * 
 * REPL = Read-Eval-Print Loop
 * This is the core pattern that interactive shells use.
 * 
 * WHAT A REPL IS:
 * A REPL is a simple interactive environment that:
 * 1. READS input from the user
 * 2. EVALUATES/parses the input 
 * 3. PRINTS the result
 * 4. LOOPs back to step 1
 * 
 * WHY SHELLS USE THIS PATTERN:
 * - Perfect for interactive use - user types, sees result, types again
 * - Simple to implement - just an infinite loop
 * - Matches how humans interact with computers (conversation)
 * - Every command is self-contained (no compile/run cycle)
 * 
 * Examples: Python REPL, JavaScript console, Ruby IRB, Bash itself
 */

#include "../include/shell.h"
#include "../include/builtins.h"
#include "../include/jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/*
 * Global shell state - kept here for simplicity
 * In production, you'd pass this around as a parameter
 */
shell_t shell_state;

/*
 * print_prompt() - Display the shell prompt
 * 
 * Format: "cshell:/home/user$ "
 * Mimics bash's "user@host:path$ " format
 */
void print_prompt(void)
{
    char cwd[MAX_LINE];
    
    /* getcwd() gets current working directory */
    /* Returns NULL on error (very rare), use "?" fallback */
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "?");
    }
    
    /* Print prompt in format: cshell:/path$ */
    printf("cshell:%s$ ", cwd);
    fflush(stdout);  /* Ensure prompt appears immediately */
}

/*
 * read_input() - Read a line of input from user
 * 
 * Uses fgets() for basic line reading:
 * - Reads from stdin (keyboard)
 * - Includes newline in result
 * - Returns NULL on EOF (Ctrl+D)
 * 
 * Returns: Dynamically allocated string (caller must free)
 *          or NULL on EOF.
 */
char *read_input(void)
{
    static char buffer[MAX_LINE];
    
    /* fgets() reads up to MAX_LINE-1 characters
     * Stops on newline or EOF
     * Returns NULL on error or EOF (when no chars read)
     */
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return NULL;
    }
    
    /* Remove trailing newline if present */
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    
    /* Return a copy (caller will free) */
    return strdup(buffer);
}

/*
 * add_to_history() - Add command to shell history
 * 
 * Uses the function from jobs.c - declared in shell.h
 */

/*
 * run_command() - Execute a command string
 * 
 * This is the "Eval" part of REPL.
 * 
 * Steps:
 * 1. Add to history
 * 2. Parse the input into command structures
 * 3. Execute the parsed commands
 * 4. Clean up memory
 */
void run_command(const char *input)
{
    command_t *cmd;
    int status;
    
    /* Skip empty input (user just pressed Enter) */
    if (!input || *input == '\0') {
        return;
    }
    
    /* Add to history */
    add_to_history(&shell_state, input);
    
    /* Parse the input into command structures
     * Converts "ls -la > out.txt" into a command_t with:
     * - argv = ["ls", "-la"]
     * - redirections = [REDIR_OUT, "out.txt"]
     */
    cmd = parse(input);
    
    if (cmd == NULL) {
        /* Parse error - parse() already printed error */
        return;
    }
    
    /* Execute the parsed commands
     * This is where fork()/execvp() happens
     */
    status = execute_command(&shell_state, cmd);
    
    /* Clean up parsed command structures */
    free_command(cmd);
}

/*
 * main() - Shell entry point
 * 
 * Performs:
 * 1. One-time initialization
 * 2. The REPL loop
 * 3. Cleanup on exit
 */
int main(int argc, char **argv)
{
    char *line = NULL;
    
    (void)argc;  /* Unused */
    (void)argv;  /* Unused */
    
    /* Initialize shell state (jobs list, etc.) */
    init_shell(&shell_state);
    
    /* Setup signal handlers (Ctrl+C, Ctrl+Z, etc.) */
    setup_signals();
    
    /* Print welcome message */
    printf("Welcome to cshell - Type 'help' for commands\n");
    
    /*
     * THE REPL LOOP
     * 
     * This is the heart of the shell:
     * while (true) {
     *     print_prompt();      // PRINT
     *     line = read_input();  // READ
     *     run_command(line);    // EVAL
     *     free(line);          // CLEANUP
     * }
     */
    while (1) {
        /* Print prompt */
        print_prompt();
        
        /* Read input line */
        line = read_input();
        
        /* Handle EOF (Ctrl+D) */
        if (line == NULL) {
            printf("\n");
            break;
        }
        
        /* Skip empty lines */
        if (*line == '\0') {
            free(line);
            continue;
        }
        
        /* Execute the command */
        run_command(line);
        
        /* Free the input line */
        free(line);
        
        /* Reap any zombie background processes
         * This is important - otherwise we'll have zombie processes
         */
        reap_zombies(&shell_state);
    }
    
    /* Clean up before exit */
    cleanup_shell(&shell_state);
    
    return 0;
}