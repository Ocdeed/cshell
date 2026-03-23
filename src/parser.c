/*
 * cshell/src/parser.c - Command parser implementation
 * 
 * RESPONSIBILITY: Convert raw input text into structured command objects
 * 
 * WHY SEPARATE: Parsing is a distinct phase from execution.
 * - It handles the grammar of shell commands
 * - It's independent of how we actually run commands
 * - Makes the code testable (can test parser without executing)
 * - Different executors can reuse the same parser
 * 
 * FUNCTIONS:
 * - parse(): main entry point
 * - tokenizer: lexical analysis
 * - command building: syntax analysis
 */

#include "../include/parser.h"
#include "../include/shell.h"
#include "../include/executor.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
 * =============================================================================
 * SIMPLE TOKENIZER - Educational Implementation
 * =============================================================================
 * 
 * This simpler function demonstrates the core concepts of tokenization.
 * It handles spaces and tabs but NOT quotes or special characters.
 * See tokenizer_next() for a full implementation.
 */

/*
 * parse_input() - Split input string into tokens
 * 
 * Input:  "ls -la /home/user"
 * Output: ["ls", "-la", "/home/user", NULL]
 * 
 * WHY THIS APPROACH (strtok vs manual):
 * 
 * Option 1: strtok()
 *   - Thread-unsafe (uses static buffer)
 *   - Modifies original string (destructive)
 *   - Simpler code
 *   
 * Option 2: Manual pointer arithmetic (what we use)
 *   - Thread-safe
 *   - Non-destructive to original
 *   - More control over memory
 * 
 * We choose manual approach because:
 *   1. Shell parsing happens frequently (every command)
 *   2. We might need the original string later (history)
 *   3. Thread safety matters for background jobs
 * 
 * MEMORY STRATEGY:
 * - Caller passes input string (we DON'T free it)
 * - We allocate memory for EACH token (must be freed by caller)
 * - We allocate the argv array (must be freed by caller)
 * 
 * Example cleanup:
 *   char **argv;
 *   int argc;
 *   parse_input(input, &argc, &argv);
 *   // use argv...
 *   for (int i = 0; i < argc; i++) free(argv[i]);
 *   free(argv);
 */
char **parse_input(char *input, int *argc)
{
    /* 
     * Step 1: Count tokens first
     * 
     * We need to know how many tokens BEFORE allocating.
     * This way we allocate exactly the right amount.
     */
    int token_count = 0;
    char *ptr = input;
    
    /* Skip leading whitespace */
    while (*ptr == ' ' || *ptr == '\t') ptr++;
    
    /* Count tokens by finding whitespace boundaries */
    while (*ptr != '\0') {
        /* Found start of a token */
        token_count++;
        
        /* Skip to end of token (next whitespace or end) */
        while (*ptr != ' ' && *ptr != '\t' && *ptr != '\0') {
            ptr++;
        }
        
        /* Skip trailing whitespace */
        while (*ptr == ' ' || *ptr == '\t') {
            ptr++;
        }
    }
    
    /*
     * Step 2: Allocate memory
     * 
     * We allocate:
     * - token_count + 1 for NULL terminator
     * - Each token needs its own allocation (strdup)
     * 
     * The +1 is for NULL termination (CRITICAL for execvp)
     */
    char **argv = malloc((token_count + 1) * sizeof(char *));
    if (!argv) return NULL;
    
    /*
     * Step 3: Extract tokens
     * 
     * We use a two-pointer approach:
     * - start: beginning of current token
     * - end: end of current token
     */
    int i = 0;
    ptr = input;
    
    /* Skip leading whitespace again */
    while (*ptr == ' ' || *ptr == '\t') ptr++;
    
    while (*ptr != '\0' && i < token_count) {
        char *start = ptr;
        size_t len = 0;
        
        /* Find length of token */
        while (ptr[len] != ' ' && ptr[len] != '\t' && ptr[len] != '\0') {
            len++;
        }
        
        /* 
         * Allocate and copy token
         * 
         * strdup() does: malloc(len + 1) + memcpy + null terminator
         * It's equivalent to:
         *   char *token = malloc(len + 1);
         *   strncpy(token, start, len);
         *   token[len] = '\0';
         */
        argv[i] = malloc(len + 1);
        if (argv[i]) {
            strncpy(argv[i], start, len);
            argv[i][len] = '\0';
        }
        i++;
        
        /* Move past this token */
        ptr += len;
        
        /* Skip whitespace to next token */
        while (*ptr == ' ' || *ptr == '\t') ptr++;
    }
    
    /* 
     * NULL TERMINATION - CRITICAL!
     * 
     * execvp() requires a NULL-terminated array!
     * Without this, execvp() will read garbage memory.
     * 
     * What NULL termination means:
     * - argv[argc] = NULL (last element is NULL)
     * - This tells execvp() where the arguments end
     * - Without it, execvp() doesn't know when to stop
     */
    argv[token_count] = NULL;
    
    /* Return token count to caller */
    if (argc) *argc = token_count;
    
    return argv;
}

/*
 * free_tokens() - Free memory allocated by parse_input()
 * 
 * MUST be called after parse_input() to prevent memory leaks!
 */
void free_tokens(char **argv)
{
    if (!argv) return;
    
    /* Free each token string */
    for (int i = 0; argv[i] != NULL; i++) {
        free(argv[i]);
    }
    
    /* Free the argv array itself */
    free(argv);
}

/*
 * =============================================================================
 * PIPE PARSING - Educational implementation
 * =============================================================================
 */

/*
 * find_pipe() - Find the | character in argv
 * 
 * Returns: index of | in argv, or -1 if not found
 * 
 * Example:
 *   argv = ["ls", "-la", "|", "grep", "c", NULL]
 *   find_pipe(argv) = 2
 */
int find_pipe(char **argv)
{
    int i;
    
    for (i = 0; argv[i] != NULL; i++) {
        if (strcmp(argv[i], "|") == 0) {
            return i;
        }
    }
    
    return -1;  /* No pipe found */
}

/*
 * split_at_pipe() - Split argv at the pipe character
 * 
 * left:  Points to left side of pipe (before |)
 * right: Points to right side of pipe (after |)
 * 
 * Example:
 *   Input:  ["ls", "-la", "|", "grep", "c", NULL]
 *   Output: left=["ls", "-la", NULL], right=["grep", "c", NULL]
 * 
 * The | token is NOT included in either side.
 * Caller is responsible for freeing both arrays.
 */
void split_at_pipe(char **argv, char ***left, char ***right)
{
    int pipe_pos = find_pipe(argv);
    
    if (pipe_pos < 0) {
        /* No pipe found - left gets everything, right is NULL */
        *left = argv;
        *right = NULL;
        return;
    }
    
    /* Count tokens on each side of pipe */
    int left_count = 0;
    for (int i = 0; i < pipe_pos; i++) {
        left_count++;
    }
    
    int right_count = 0;
    for (int i = pipe_pos + 1; argv[i] != NULL; i++) {
        right_count++;
    }
    
    /* Allocate left array (NULL-terminated) */
    *left = malloc((left_count + 1) * sizeof(char *));
    for (int i = 0; i < left_count; i++) {
        (*left)[i] = argv[i];
    }
    (*left)[left_count] = NULL;
    
    /* Allocate right array (NULL-terminated) */
    if (right_count > 0) {
        *right = malloc((right_count + 1) * sizeof(char *));
        for (int i = 0; i < right_count; i++) {
            (*right)[i] = argv[pipe_pos + 1 + i];
        }
        (*right)[right_count] = NULL;
    } else {
        *right = NULL;
    }
}

/*
 * run_piped_command() - Parse and execute a piped command
 * 
 * This is the SIMPLE version that handles ONE pipe.
 * For multiple pipes, you'd call this recursively.
 * 
 * Input: "ls -la | grep c"
 * 1. Parse into tokens: ["ls", "-la", "|", "grep", "c"]
 * 2. Split at pipe
 * 3. Execute with pipe
 */
void run_piped_command(char *input)
{
    int argc;
    char **argv;
    char **left, **right;
    
    /* Parse input into tokens */
    argv = parse_input(input, &argc);
    if (!argv || argc == 0) {
        return;
    }
    
    /* Find pipe position */
    int pipe_pos = find_pipe(argv);
    
    if (pipe_pos < 0) {
        /* No pipe - execute normally */
        simple_execute(argv);
        free_tokens(argv);
        return;
    }
    
    if (pipe_pos == 0) {
        fprintf(stderr, "pipe: syntax error near unexpected token '|'\n");
        free_tokens(argv);
        return;
    }
    
    if (argv[pipe_pos + 1] == NULL) {
        fprintf(stderr, "pipe: syntax error near unexpected token '|'\n");
        free_tokens(argv);
        return;
    }
    
    /* Split at pipe */
    split_at_pipe(argv, &left, &right);
    
    /* Execute with pipe */
    execute_piped(left, right);
    
    /* Free split arrays (but not the strings - they belong to argv) */
    free(left);
    if (right) free(right);
    
    /* Free original argv */
    free_tokens(argv);
}

/*
 * =============================================================================
 * FULL TOKENIZER - Handles quotes, pipes, redirects, etc.
 * =============================================================================
 */

/*
 * tokenizer_create() - Create a new tokenizer
 * 
 * Simple wrapper that allocates and initializes tokenizer struct.
 */
tokenizer_t *tokenizer_create(const char *input)
{
    tokenizer_t *tk = malloc(sizeof(tokenizer_t));
    if (!tk) return NULL;
    
    tk->input = input;
    tk->pos = 0;
    tk->len = strlen(input);
    
    return tk;
}

/*
 * tokenizer_destroy() - Free tokenizer memory
 */
void tokenizer_destroy(tokenizer_t *tk)
{
    free(tk);
}

/*
 * skip_whitespace() - Skip spaces and tabs in input
 * Returns pointer to next non-whitespace character
 */
static const char *skip_whitespace(const char *s)
{
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

/*
 * tokenizer_next() - Get the next token from input
 * 
 * This is the lexical analyzer. It reads characters and
 * categorizes them into tokens.
 * 
 * Handle cases:
 * - Word: alphanumerics and most special chars (abc, ls, /path)
 * - "quoted string": everything inside quotes
 * - | : pipe operator
 * - <, >, >> : redirection
 * - & : background
 * - ; : command separator
 */
token_t *tokenizer_next(tokenizer_t *tk)
{
    token_t *token;
    const char *start;
    size_t len;
    
    /* Skip leading whitespace */
    tk->input = skip_whitespace(tk->input);
    
    /* Check for end of input */
    if (*tk->input == '\0') {
        token = malloc(sizeof(token_t));
        token->type = TOKEN_EOF;
        token->value = NULL;
        return token;
    }
    
    /* Allocate token */
    token = malloc(sizeof(token_t));
    if (!token) return NULL;
    
    /* Check for special single-character tokens */
    switch (*tk->input) {
        case '|':
            token->type = TOKEN_PIPE;
            token->value = NULL;
            tk->input++;
            return token;
            
        case '<':
            token->type = TOKEN_REDIRECT_IN;
            token->value = NULL;
            tk->input++;
            return token;
            
        case '>':
            if (tk->input[1] == '>') {
                token->type = TOKEN_REDIR_APPEND;
                tk->input += 2;
            } else {
                token->type = TOKEN_REDIRECT_OUT;
                tk->input++;
            }
            token->value = NULL;
            return token;
            
        case '&':
            token->type = TOKEN_AMPERSAND;
            token->value = NULL;
            tk->input++;
            return token;
            
        case ';':
            token->type = TOKEN_SEMICOLON;
            token->value = NULL;
            tk->input++;
            return token;
    }
    
    /* Handle quoted strings */
    if (*tk->input == '"' || *tk->input == '\'') {
        char quote = *tk->input;
        tk->input++;
        start = tk->input;
        
        while (*tk->input && *tk->input != quote) {
            tk->input++;
        }
        
        len = tk->input - start;
        token->value = malloc(len + 1);
        strncpy(token->value, start, len);
        token->value[len] = '\0';
        token->type = TOKEN_WORD;
        
        if (*tk->input) tk->input++; /* Skip closing quote */
        return token;
    }
    
    /* Regular word - read until whitespace or special char */
    start = tk->input;
    while (*tk->input && *tk->input != ' ' && *tk->input != '\t' &&
           *tk->input != '|' && *tk->input != '<' && *tk->input != '>' &&
           *tk->input != '&' && *tk->input != ';') {
        tk->input++;
    }
    
    len = tk->input - start;
    token->value = malloc(len + 1);
    strncpy(token->value, start, len);
    token->value[len] = '\0';
    token->type = TOKEN_WORD;
    
    return token;
}

/*
 * token_destroy() - Free token memory
 */
void token_destroy(token_t *token)
{
    if (token->value) free(token->value);
    free(token);
}

/*
 * command_create() - Create an empty command structure
 */
command_t *command_create(void)
{
    command_t *cmd = calloc(1, sizeof(command_t));
    return cmd;
}

/*
 * command_add_arg() - Add an argument to a command
 */
void command_add_arg(command_t *cmd, char *arg)
{
    if (cmd->argc >= MAX_ARGS - 1) return;
    cmd->argv[cmd->argc++] = arg;
    cmd->argv[cmd->argc] = NULL;
}

/*
 * command_add_redirect() - Add a redirection to a command
 */
void command_add_redirect(command_t *cmd, redir_type_t type, char *filename)
{
    /* Expand redirection array if needed */
    if (cmd->num_redirs == 0) {
        cmd->redirs = malloc(sizeof(redir_t));
    } else {
        cmd->redirs = realloc(cmd->redirs, sizeof(redir_t) * (cmd->num_redirs + 1));
    }
    
    cmd->redirs[cmd->num_redirs].type = type;
    cmd->redirs[cmd->num_redirs].filename = filename;
    cmd->redirs[cmd->num_redirs].fd = -1; /* Will be set based on type */
    cmd->num_redirs++;
}

/*
 * parse() - Main parsing function
 * 
 * Takes raw input string and returns linked list of commands.
 * 
 * Algorithm:
 * 1. Tokenize input
 * 2. For each command:
 *    - Collect arguments until pipe/semicolon/EOF
 *    - Attach redirections
 * 3. Link commands together for pipes
 */
command_t *parse(const char *input)
{
    tokenizer_t *tk;
    token_t *token;
    command_t *head = NULL;
    command_t *current = NULL;
    command_t *cmd = NULL;
    
    if (!input || *input == '\0') return NULL;
    
    tk = tokenizer_create(input);
    
    cmd = command_create();
    head = current = cmd;
    
    /* Main token loop */
    while ((token = tokenizer_next(tk)) != NULL) {
        if (token->type == TOKEN_EOF) {
            token_destroy(token);
            break;
        }
        
        switch (token->type) {
            case TOKEN_WORD:
                command_add_arg(cmd, token->value);
                free(token);
                break;
                
            case TOKEN_PIPE:
                /* Create next command in pipeline */
                current->next = command_create();
                current = current->next;
                cmd = current;
                token_destroy(token);
                break;
                
            case TOKEN_REDIRECT_IN:
                token = tokenizer_next(tk);
                if (token && token->type == TOKEN_WORD) {
                    command_add_redirect(cmd, REDIR_IN, token->value);
                    free(token);
                }
                break;
                
            case TOKEN_REDIRECT_OUT:
                token = tokenizer_next(tk);
                if (token && token->type == TOKEN_WORD) {
                    command_add_redirect(cmd, REDIR_OUT, token->value);
                    free(token);
                }
                break;
                
            case TOKEN_REDIR_APPEND:
                token = tokenizer_next(tk);
                if (token && token->type == TOKEN_WORD) {
                    command_add_redirect(cmd, REDIR_APPEND, token->value);
                    free(token);
                }
                break;
                
            case TOKEN_AMPERSAND:
                cmd->num_redirs = -1; /* Marker for background */
                token_destroy(token);
                break;
                
            case TOKEN_SEMICOLON:
                /* Command separator - create new command */
                current->next = command_create();
                current = current->next;
                cmd = current;
                token_destroy(token);
                break;
                
            default:
                token_destroy(token);
                break;
        }
    }
    
    tokenizer_destroy(tk);
    
    return head;
}

/*
 * free_command() - Free a single command structure
 */
void free_command(command_t *cmd)
{
    int i;
    
    if (!cmd) return;
    
    /* Free arguments */
    for (i = 0; i < cmd->argc; i++) {
        free(cmd->argv[i]);
    }
    
    /* Free redirections */
    if (cmd->redirs) {
        for (i = 0; i < cmd->num_redirs; i++) {
            free(cmd->redirs[i].filename);
        }
        free(cmd->redirs);
    }
    
    free(cmd);
}

/*
 * free_pipeline() - Free entire command pipeline
 */
void free_pipeline(command_t *head)
{
    command_t *current = head;
    command_t *next;
    
    while (current) {
        next = current->next;
        free_command(current);
        current = next;
    }
}