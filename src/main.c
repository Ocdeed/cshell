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
#include "../include/parser.h"
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
 * =============================================================================
 * PARSER TEST - Demonstrates tokenization
 * =============================================================================
 * 
 * Run with: ./cshell --test-parser
 * 
 * This tests the parse_input() function with various inputs.
 */

/*
 * test_parser() - Test the parse_input() function
 * 
 * Demonstrates:
 * - Basic space-separated tokens
 * - Multiple spaces/tabs
 * - Empty input
 * - Single word
 * - Preserves original input (non-destructive)
 */
void test_parser(void)
{
    printf("\n=== PARSER TEST ===\n\n");
    
    /* Test case 1: Basic tokens with multiple spaces */
    printf("Test 1: Multiple spaces between words\n");
    printf("Input:  \"ls    -la    /home\"\n");
    char input1[] = "ls    -la    /home";
    int argc1;
    char **argv1 = parse_input(input1, &argc1);
    printf("Tokens (%d):\n", argc1);
    for (int i = 0; i < argc1; i++) {
        printf("  [%d] \"%s\"\n", i, argv1[i]);
    }
    printf("NULL terminator: %s\n\n", argv1[argc1] == NULL ? "OK" : "MISSING!");
    free_tokens(argv1);
    
    /* Test case 2: Leading and trailing whitespace */
    printf("Test 2: Leading and trailing whitespace\n");
    printf("Input:  \"   echo hello   \\t  world   \"\n");
    char input2[] = "   echo hello     world   ";
    int argc2;
    char **argv2 = parse_input(input2, &argc2);
    printf("Tokens (%d):\n", argc2);
    for (int i = 0; i < argc2; i++) {
        printf("  [%d] \"%s\"\n", i, argv2[i]);
    }
    free_tokens(argv2);
    printf("\n");
    
    /* Test case 3: Single word */
    printf("Test 3: Single word\n");
    printf("Input:  \"ls\"\n");
    char input3[] = "ls";
    int argc3;
    char **argv3 = parse_input(input3, &argc3);
    printf("Tokens (%d):\n", argc3);
    for (int i = 0; i < argc3; i++) {
        printf("  [%d] \"%s\"\n", i, argv3[i]);
    }
    free_tokens(argv3);
    printf("\n");
    
    /* Test case 4: Empty string */
    printf("Test 4: Empty string\n");
    printf("Input:  \"\"\n");
    char input4[] = "";
    int argc4;
    char **argv4 = parse_input(input4, &argc4);
    printf("Tokens (%d):\n", argc4);
    printf("  (no tokens)\n");
    if (argv4) free_tokens(argv4);
    printf("\n");
    
    /* Test case 5: Original input preserved */
    printf("Test 5: Original input preserved\n");
    printf("Input:  \"original string\"\n");
    char input5[] = "original string";
    int argc5;
    char **argv5 = parse_input(input5, &argc5);
    printf("After parse, input is still: \"%s\"\n", input5);
    printf("Note: We use strncpy, so original is NOT modified.\n");
    free_tokens(argv5);
    printf("\n");
    
    /* Test case 6: Path with spaces (NOT quoted - shows limitation) */
    printf("Test 6: Path with spaces (educational - shows limitation)\n");
    printf("Input:  \"/path/with spaces\"\n");
    printf("Note:   Simple parser treats each word separately.\n");
    printf("        Full parser would handle quotes: \"quoted string\"\n");
    char input6[] = "/path/with spaces";
    int argc6;
    char **argv6 = parse_input(input6, &argc6);
    printf("Tokens (%d):\n", argc6);
    for (int i = 0; i < argc6; i++) {
        printf("  [%d] \"%s\"\n", i, argv6[i]);
    }
    printf("This shows why we need QUOTE HANDLING for paths with spaces!\n");
    free_tokens(argv6);
    printf("\n");
    
    printf("=== END PARSER TEST ===\n\n");
}

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
 * 1. Command-line argument parsing
 * 2. One-time initialization
 * 3. The REPL loop (or tests)
 * 4. Cleanup on exit
 */
int main(int argc, char **argv)
{
    char *line = NULL;
    
    /* Check for test mode */
    if (argc > 1 && strcmp(argv[1], "--test-parser") == 0) {
        test_parser();
        return 0;
    }
    
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