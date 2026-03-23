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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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