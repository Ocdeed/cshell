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
#include "../include/executor.h"
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
 * =============================================================================
 * EXECUTOR TEST - Demonstrates fork/exec/wait
 * =============================================================================
 * 
 * Run with: ./cshell --test-executor
 */

/*
 * test_executor() - Test the simple_execute() function
 * 
 * Demonstrates:
 * - fork() creates child process
 * - execvp() runs external command
 * - waitpid() waits for child
 */
void test_executor(void)
{
    printf("\n=== EXECUTOR TEST ===\n\n");
    fflush(stdout);
    
    /*
     * TEST 1: Simple ls command
     * 
     * We pass: ["ls", "-l", NULL]
     * execvp() searches PATH and runs /bin/ls -l
     */
    printf("Test 1: Running 'ls -l'\n");
    fflush(stdout);
    char *args1[] = {"ls", "-l", NULL};
    simple_execute(args1);
    printf("Returned from simple_execute()\n\n");
    fflush(stdout);
    
    /*
     * TEST 2: pwd command
     * 
     * Demonstrates that execvp() works with PATH search
     */
    printf("Test 2: Running 'pwd'\n");
    fflush(stdout);
    char *args2[] = {"pwd", NULL};
    simple_execute(args2);
    printf("Returned from simple_execute()\n\n");
    fflush(stdout);
    
    /*
     * TEST 3: echo with arguments
     * 
     * The shell parses "echo hello world" into:
     * ["echo", "hello", "world", NULL]
     */
    printf("Test 3: Running 'echo hello world'\n");
    fflush(stdout);
    char *args3[] = {"echo", "hello", "world", NULL};
    simple_execute(args3);
    printf("Returned from simple_execute()\n\n");
    fflush(stdout);
    
    /*
     * TEST 4: Command not found
     * 
     * What happens when execvp() fails?
     * - execvp() returns -1
     * - Child prints error and exits
     * - Parent continues normally
     */
    printf("Test 4: Running 'nonexistent_command'\n");
    fflush(stdout);
    printf("This should print an error...\n");
    fflush(stdout);
    char *args4[] = {"nonexistent_command", NULL};
    simple_execute(args4);
    printf("Parent continues after failed exec!\n\n");
    fflush(stdout);
    
    /*
     * WHAT YOU JUST SAW:
     * 
     * Each simple_execute() call:
     * 1. fork() created a child process
     * 2. Child ran the command (ls, pwd, echo)
     * 3. Parent waited for child to finish
     * 4. Control returned to shell
     */
    
    printf("=== END EXECUTOR TEST ===\n\n");
}

/*
 * =============================================================================
 * BUILTIN TEST - Demonstrates built-in commands
 * =============================================================================
 * 
 * Run with: ./cshell --test-builtins
 */

/*
 * test_builtins() - Test built-in commands
 * 
 * Demonstrates:
 * - Built-ins run IN the shell process (no fork!)
 * - cd changes shell's working directory
 * - exit codes: 0 = success, non-zero = error
 */
void test_builtins(void)
{
    char *args[3];
    int result;
    
    printf("\n=== BUILTIN TEST ===\n\n");
    fflush(stdout);
    
    /* 
     * TEST 1: Check if command is built-in
     * 
     * simple_is_builtin() decides: fork() or not?
     */
    printf("Test 1: Checking if commands are built-in\n");
    printf("  simple_is_builtin(\"cd\") = %d (should be 1)\n", 
           simple_is_builtin("cd"));
    printf("  simple_is_builtin(\"pwd\") = %d (should be 1)\n", 
           simple_is_builtin("pwd"));
    printf("  simple_is_builtin(\"ls\") = %d (should be 0 - external!)\n", 
           simple_is_builtin("ls"));
    printf("  simple_is_builtin(\"echo\") = %d (should be 1)\n", 
           simple_is_builtin("echo"));
    printf("\n");
    
    /*
     * TEST 2: pwd - Print working directory
     * 
     * Note: This COULD be /bin/pwd (external).
     * Built-in version is faster (no fork/exec).
     */
    printf("Test 2: Running 'pwd' (built-in)\n");
    printf("  Note: This runs IN the shell process, not forked!\n");
    args[0] = "pwd";
    args[1] = NULL;
    result = simple_run_builtin(args);
    printf("  Return value: %d (0 = success)\n\n", result);
    fflush(stdout);
    
    /*
     * TEST 3: cd - Change directory
     * 
     * WHY THIS MUST BE BUILT-IN:
     * External /bin/cd would only change ITS working directory.
     * Built-in changes THE SHELL'S working directory!
     */
    printf("Test 3: Running 'cd /tmp' (built-in)\n");
    printf("  Before: ");
    fflush(stdout);
    args[0] = "pwd"; args[1] = NULL;
    simple_run_builtin(args);
    
    args[0] = "cd";
    args[1] = "/tmp";
    args[2] = NULL;
    result = simple_run_builtin(args);
    
    printf("  After: ");
    fflush(stdout);
    args[0] = "pwd"; args[1] = NULL;
    simple_run_builtin(args);
    printf("  Return value: %d\n\n", result);
    
    /*
     * TEST 4: cd with no argument (go to HOME)
     */
    printf("Test 4: Running 'cd' (no argument - goes to HOME)\n");
    args[0] = "cd";
    args[1] = NULL;
    result = simple_run_builtin(args);
    printf("  Return value: %d\n\n", result);
    fflush(stdout);
    
    /*
     * TEST 5: cd with error
     * 
     * EXIT CODE: Return 1 means error occurred
     * This allows shell to detect failures.
     */
    printf("Test 5: Running 'cd /nonexistent/directory' (should fail)\n");
    args[0] = "cd";
    args[1] = "/nonexistent/directory";
    args[2] = NULL;
    result = simple_run_builtin(args);
    printf("  Return value: %d (non-zero = error!)\n\n", result);
    fflush(stdout);
    
    /*
     * TEST 6: echo with $VAR expansion
     */
    printf("Test 6: Running 'echo $HOME'\n");
    args[0] = "echo";
    args[1] = "$HOME";
    args[2] = NULL;
    simple_run_builtin(args);
    printf("\n");
    fflush(stdout);
    
    /*
     * TEST 7: help
     */
    printf("Test 7: Running 'help'\n");
    args[0] = "help";
    args[1] = NULL;
    simple_run_builtin(args);
    printf("\n");
    fflush(stdout);
    
    /*
     * KEY INSIGHT:
     * 
     * Built-ins vs External Commands:
     * 
     * Built-in (cd, pwd, echo, exit):
     * - Run in SHELL process (no fork!)
     * - CAN modify shell state (cd changes cwd)
     * - Faster (no fork/exec overhead)
     * 
     * External (/bin/ls, /bin/cat):
     * - Run in CHILD process (fork required)
     * - CANNOT modify shell state
     * - Independent programs
     */
    printf("KEY INSIGHT:\n");
    printf("  Built-ins run IN shell process - can modify shell state!\n");
    printf("  External commands run in forked child - cannot affect shell!\n\n");
    
    printf("=== END BUILTIN TEST ===\n\n");
}

/*
 * =============================================================================
 * PIPE TEST - Demonstrates pipe implementation
 * =============================================================================
 * 
 * Run with: ./cshell --test-pipe
 */

/*
 * test_pipe() - Test the execute_piped() function
 * 
 * Demonstrates:
 * - Creating a pipe with pipe()
 * - Forking two processes
 * - dup2() to redirect stdin/stdout
 * - Closing unused pipe ends
 * 
 * ASCII FLOW:
 * 
 *   ls PROCESS              KERNEL PIPE           grep PROCESS
 *   ┌─────────┐          ┌─────────┐          ┌─────────┐
 *   │ stdout   │──────────│ BUFFER  │──────────│ stdin   │
 *   │ =pipe[1]│──write──▶│         │──read───│ =pipe[0]│
 *   └─────────┘          └─────────┘          └─────────┘
 */
void test_pipe(void)
{
    printf("\n=== PIPE TEST ===\n\n");
    fflush(stdout);
    
    /*
     * TEST 1: ls | grep
     * 
     * This is THE classic pipe example.
     * 
     * What happens:
     * 1. ls writes filenames to stdout
     * 2. Pipe captures stdout
     * 3. grep reads from stdin (pipe output)
     * 4. grep filters for lines containing "c"
     */
    printf("Test 1: Running 'ls | grep c'\n");
    printf("  ls writes filenames → pipe → grep reads → filters for 'c'\n");
    printf("  Implementation:\n");
    printf("    1. pipe(pipefd) creates IPC channel\n");
    printf("    2. fork() creates ls process\n");
    printf("    3. dup2(pipefd[1], STDOUT) redirects ls output\n");
    printf("    4. fork() creates grep process\n");
    printf("    5. dup2(pipefd[0], STDIN) redirects grep input\n");
    printf("    6. Parent waits for both children\n\n");
    fflush(stdout);
    
    char *left1[] = {"ls", NULL};
    char *right1[] = {"grep", "c", NULL};
    execute_piped(left1, right1);
    
    printf("\n");
    
    /*
     * TEST 2: echo | wc
     * 
     * Count lines in "hello world"
     */
    printf("Test 2: Running 'echo hello world | wc -l'\n");
    printf("  Counts number of lines\n\n");
    fflush(stdout);
    
    char *left2[] = {"echo", "hello", "world", NULL};
    char *right2[] = {"wc", "-l", NULL};
    execute_piped(left2, right2);
    
    printf("\n");
    
    /*
     * TEST 3: cat | head
     * 
     * Show first 3 lines of /etc/passwd
     */
    printf("Test 3: Running 'cat /etc/passwd | head -3'\n");
    printf("  Shows first 3 lines of /etc/passwd\n\n");
    fflush(stdout);
    
    char *left3[] = {"cat", "/etc/passwd", NULL};
    char *right3[] = {"head", "-3", NULL};
    execute_piped(left3, right3);
    
    printf("\n");
    
    /*
     * TEST 4: ls -l | tail -5
     * 
     * Show last 5 files (using tail on ls output)
     */
    printf("Test 4: Running 'ls -l | tail -5'\n");
    printf("  Shows last 5 items in current directory\n\n");
    fflush(stdout);
    
    char *left4[] = {"ls", "-l", NULL};
    char *right4[] = {"tail", "-5", NULL};
    execute_piped(left4, right4);
    
    printf("\n");
    
    /*
     * WHAT YOU JUST SAW:
     * 
     * The execute_piped() function:
     * 1. Creates pipe with pipe()
     * 2. Forks left process (producer)
     *    - dup2(pipefd[1], STDOUT_FILENO)
     *    - close(pipefd[0]) - don't read
     *    - execvp(left_args)
     * 3. Forks right process (consumer)
     *    - dup2(pipefd[0], STDIN_FILENO)
     *    - close(pipefd[1]) - don't write
     *    - execvp(right_args)
     * 4. Parent closes both ends, waits for children
     * 
     * THE CRITICAL PART:
     * Closing unused pipe ends is essential!
     * If you don't close pipefd[1] in grep,
     * grep will never see EOF and will hang forever.
     */
    printf("KEY INSIGHT:\n");
    printf("  Pipes connect stdout of LEFT to stdin of RIGHT\n");
    printf("  dup2() redirects file descriptors\n");
    printf("  ALWAYS close unused pipe ends!\n\n");
    
    printf("=== END PIPE TEST ===\n\n");
}

/*
 * =============================================================================
 * PIPE PARSER TEST - Demonstrates parsing piped commands
 * =============================================================================
 * 
 * Run with: ./cshell --test-pipe-parse
 */

/*
 * test_pipe_parse() - Test parsing piped commands
 * 
 * Demonstrates:
 * - Finding | in token array
 * - Splitting argv at pipe position
 * - Executing with pipe
 */
void test_pipe_parse(void)
{
    char **left, **right;
    char input[MAX_LINE];
    
    printf("\n=== PIPE PARSER TEST ===\n\n");
    
    /*
     * TEST 1: Parse "ls | grep c"
     */
    printf("Test 1: Parsing 'ls | grep c'\n");
    strcpy(input, "ls | grep c");
    
    int argc;
    char **argv = parse_input(input, &argc);
    printf("  Tokens: ");
    for (int i = 0; i < argc; i++) {
        printf("[%s] ", argv[i]);
    }
    printf("\n");
    
    int pipe_pos = find_pipe(argv);
    printf("  Pipe position: %d\n", pipe_pos);
    
    split_at_pipe(argv, &left, &right);
    printf("  Left side: ");
    for (int i = 0; left[i]; i++) printf("[%s] ", left[i]);
    printf("\n");
    printf("  Right side: ");
    for (int i = 0; right && right[i]; i++) printf("[%s] ", right[i]);
    printf("\n\n");
    
    printf("  Executing...\n");
    execute_piped(left, right);
    
    free(left);
    if (right) free(right);
    free_tokens(argv);
    
    /*
     * TEST 2: Parse "cat /etc/passwd | head -5"
     */
    printf("\nTest 2: Parsing 'cat /etc/passwd | head -5'\n");
    strcpy(input, "cat /etc/passwd | head -5");
    
    argv = parse_input(input, &argc);
    printf("  Tokens: ");
    for (int i = 0; i < argc; i++) {
        printf("[%s] ", argv[i]);
    }
    printf("\n");
    
    split_at_pipe(argv, &left, &right);
    printf("  Left: "); for (int i = 0; left[i]; i++) printf("[%s] ", left[i]);
    printf("\n");
    printf("  Right: "); for (int i = 0; right && right[i]; i++) printf("[%s] ", right[i]);
    printf("\n\n");
    
    printf("  Executing...\n");
    execute_piped(left, right);
    
    free(left);
    if (right) free(right);
    free_tokens(argv);
    
    /*
     * TEST 3: Command without pipe
     */
    printf("\nTest 3: Parsing 'ls' (no pipe)\n");
    strcpy(input, "ls");
    
    argv = parse_input(input, &argc);
    split_at_pipe(argv, &left, &right);
    printf("  Left: "); for (int i = 0; left[i]; i++) printf("[%s] ", left[i]);
    printf("\n");
    printf("  Right: %s\n\n", right ? "exists" : "NULL");
    
    printf("  Executing (simple, no pipe)...\n");
    simple_execute(left);
    
    free(left);
    if (right) free(right);
    free_tokens(argv);
    
    printf("\n=== END PIPE PARSER TEST ===\n\n");
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
    if (argc > 1) {
        if (strcmp(argv[1], "--test-parser") == 0) {
            test_parser();
            return 0;
        }
        if (strcmp(argv[1], "--test-executor") == 0) {
            test_executor();
            return 0;
        }
        if (strcmp(argv[1], "--test-builtins") == 0) {
            test_builtins();
            return 0;
        }
        if (strcmp(argv[1], "--test-pipe") == 0) {
            test_pipe();
            return 0;
        }
        if (strcmp(argv[1], "--test-pipe-parse") == 0) {
            test_pipe_parse();
            return 0;
        }
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