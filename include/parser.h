/*
 * cshell/include/parser.h - Parser module header
 * 
 * The parser is responsible for converting raw input text into
 * structured command objects that the executor can understand.
 */

#ifndef CSHELL_PARSER_H
#define CSHELL_PARSER_H

#include "shell.h"

/*
 * =============================================================================
 * SIMPLE TOKENIZER - Educational Implementation
 * =============================================================================
 */

/*
 * parse_input() - Split input string into tokens
 * 
 * Input:  "ls -la /home/user"
 * Output: ["ls", "-la", "/home/user", NULL]
 * 
 * The returned array is NULL-terminated (required for execvp()).
 * 
 * Memory ownership:
 * - Caller provides input string (don't free)
 * - Caller must call free_tokens() when done
 * 
 * Usage:
 *   char **argv;
 *   int argc;
 *   argv = parse_input(input, &argc);
 *   // use argv...
 *   free_tokens(argv);
 */
char **parse_input(char *input, int *argc);

/*
 * free_tokens() - Free memory from parse_input()
 * 
 * Must be called to prevent memory leaks!
 */
void free_tokens(char **argv);

/*
 * find_pipe() - Find the | character in argv
 * 
 * Returns: index of |, or -1 if not found
 */
int find_pipe(char **argv);

/*
 * split_at_pipe() - Split argv at the pipe character
 * 
 * left:  Left side of pipe (before |)
 * right: Right side of pipe (after |)
 */
void split_at_pipe(char **argv, char ***left, char ***right);

/*
 * run_piped_command() - Parse and execute a piped command
 * 
 * Handles one pipe: left | right
 */
void run_piped_command(char *input);

/*
 * =============================================================================
 * FULL TOKENIZER - Handles quotes, pipes, redirects, etc.
 * =============================================================================
 */

/*
 * Token types - what kind of "word" we found in the input
 * Used by the tokenizer to categorize each piece of the input
 */
typedef enum {
    TOKEN_WORD,         /* Regular word (command, argument) */
    TOKEN_PIPE,        /* | character */
    TOKEN_REDIRECT_IN, /* < character */
    TOKEN_REDIRECT_OUT,/* > character */
    TOKEN_REDIR_APPEND,/* >> characters */
    TOKEN_AMPERSAND,   /* & for background execution */
    TOKEN_SEMICOLON,   /* ; for command chaining */
    TOKEN_EOF,         /* End of input */
    TOKEN_ERROR        /* Parse error */
} token_type_t;

/*
 * Token structure - a single lexical unit from input
 */
typedef struct {
    token_type_t type;  /* What kind of token this is */
    char *value;        /* The actual text (for words) */
} token_t;

/*
 * Tokenizer - converts raw input string into token stream
 * This is the first phase of parsing (lexical analysis)
 */
typedef struct tokenizer {
    const char *input;  /* Original input string */
    size_t pos;         /* Current position in input */
    size_t len;         /* Length of input */
} tokenizer_t;

/*
 * parse() - Main entry point
 * Input: raw command line string
 * Output: linked list of command structures (for pipelines)
 * 
 * The parser performs these steps:
 * 1. Tokenize input into lexical tokens
 * 2. Build command structures from tokens
 * 3. Attach redirections and arguments to commands
 * 4. Link commands together for pipes
 */
command_t *parse(const char *input);

/*
 * tokenizer functions - for internal parser use
 */
tokenizer_t *tokenizer_create(const char *input);
void tokenizer_destroy(tokenizer_t *tk);
token_t *tokenizer_next(tokenizer_t *tk);
void token_destroy(token_t *token);

/*
 * Helper functions for building command structures
 */
command_t *command_create(void);
void command_add_arg(command_t *cmd, char *arg);
void command_add_redirect(command_t *cmd, redir_type_t type, char *filename);

/*
 * Memory cleanup
 */
void free_command(command_t *cmd);
void free_pipeline(command_t *head);

#endif /* CSHELL_PARSER_H */