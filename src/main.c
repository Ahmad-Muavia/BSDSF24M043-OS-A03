
#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <ctype.h>

#define HISTORY_SIZE 20
#define MAX_BLOCK_LINES 100
#define MAX_VARIABLES 100
#define MAX_VAR_NAME 64

typedef struct {
    char name[MAX_VAR_NAME];
    char value[MAX_LEN];
} ShellVariable;

static ShellVariable shell_variables[MAX_VARIABLES];
static int variable_count = 0;

char command_history[HISTORY_SIZE][MAX_LEN];
int history_count = 0;
int total_commands = 0;

/* Check valid variable-name characters */
int is_name_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

int is_name_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

int valid_variable_name(const char *name, size_t length) {
    if (length == 0 || length >= MAX_VAR_NAME)
        return 0;

    if (!is_name_start(name[0]))
        return 0;

    for (size_t i = 1; i < length; i++) {
        if (!is_name_char(name[i]))
            return 0;
    }

    return 1;
}

/* Find an existing variable */
static int find_variable(const char *name) {
    for (int i = 0; i < variable_count; i++) {
        if (strcmp(shell_variables[i].name, name) == 0)
            return i;
    }

    return -1;
}

/* Store or update a variable */
int store_variable(const char *name, const char *value) {
    int index = find_variable(name);

    if (index == -1) {
        if (variable_count >= MAX_VARIABLES) {
            fprintf(stderr, "Maximum variable limit reached.\n");
            return 1;
        }

        index = variable_count++;
        snprintf(shell_variables[index].name,
                 MAX_VAR_NAME, "%s", name);
    }

    snprintf(shell_variables[index].value,
             MAX_LEN, "%s", value);

    return 0;
}

/* Get a variable's value */
static const char *get_variable(const char *name) {
    int index = find_variable(name);

    if (index != -1)
        return shell_variables[index].value;

    return getenv(name);
}

/* Append text to a dynamically allocated string */
static int append_text(char **buffer, size_t *length,
                       size_t *capacity, const char *text,
                       size_t text_length) {
    if (*length + text_length + 1 > *capacity) {
        size_t new_capacity = *capacity;

        while (*length + text_length + 1 > new_capacity)
            new_capacity *= 2;

        char *new_buffer = realloc(*buffer, new_capacity);

        if (new_buffer == NULL)
            return 0;

        *buffer = new_buffer;
        *capacity = new_capacity;
    }

    memcpy(*buffer + *length, text, text_length);
    *length += text_length;
    (*buffer)[*length] = '\0';

    return 1;
}

/* Expand $VARIABLE and ${VARIABLE} */
char *expand_variables(const char *input) {
    size_t capacity = MAX_LEN;
    size_t length = 0;

    char *output = malloc(capacity);

    if (output == NULL)
        return NULL;

    output[0] = '\0';

    for (size_t i = 0; input[i] != '\0';) {
        if (input[i] != '$') {
            if (!append_text(&output, &length, &capacity,
                             input + i, 1))
                goto error;

            i++;
            continue;
        }

        size_t start;
        size_t end;
        int braced = 0;

        if (input[i + 1] == '{') {
            braced = 1;
            start = i + 2;
            end = start;

            while (input[end] != '\0' && input[end] != '}')
                end++;

            if (input[end] != '}') {
                if (!append_text(&output, &length, &capacity,
                                 "$", 1))
                    goto error;

                i++;
                continue;
            }
        } else {
            start = i + 1;
            end = start;

            if (!is_name_start(input[start])) {
                if (!append_text(&output, &length, &capacity,
                                 "$", 1))
                    goto error;

                i++;
                continue;
            }

            while (is_name_char(input[end]))
                end++;
        }

        size_t name_length = end - start;

        if (valid_variable_name(input + start, name_length)) {
            char name[MAX_VAR_NAME];

            memcpy(name, input + start, name_length);
            name[name_length] = '\0';

            const char *value = get_variable(name);

            if (value != NULL) {
                if (!append_text(&output, &length, &capacity,
                                 value, strlen(value)))
                    goto error;
            }
        } else {
            if (!append_text(&output, &length, &capacity,
                             input + i, end - i + (braced ? 1 : 0)))
                goto error;
        }

        i = braced ? end + 1 : end;
    }

    return output;

error:
    free(output);
    return NULL;
}

/* Detect and process NAME=value assignments */
int handle_variable_assignment(const char *line) {
    const char *equal = strchr(line, '=');

    if (equal == NULL)
        return -1;

    size_t name_length = equal - line;

    if (!valid_variable_name(line, name_length))
        return -1;

    char name[MAX_VAR_NAME];

    memcpy(name, line, name_length);
    name[name_length] = '\0';

    char *expanded_value = expand_variables(equal + 1);

    if (expanded_value == NULL) {
        perror("Variable expansion");
        return 1;
    }

    int result = store_variable(name, expanded_value);

    free(expanded_value);

    return result;
}

/* Display all shell variables */
void show_variables(void) {
    for (int i = 0; i < variable_count; i++) {
        printf("%s=%s\n",
               shell_variables[i].name,
               shell_variables[i].value);
    }
}

/* Store command history */
void store_history(const char *command) {
    if (history_count == HISTORY_SIZE) {
        memmove(command_history, command_history + 1,
                sizeof(command_history[0]) * (HISTORY_SIZE - 1));
    } else {
        history_count++;
    }

    snprintf(command_history[history_count - 1],
             MAX_LEN, "%s", command);

    total_commands++;
}

void show_history(void) {
    int first_number = total_commands - history_count + 1;

    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n",
               first_number + i, command_history[i]);
    }
}

char *get_history(int number) {
    int first_number = total_commands - history_count + 1;

    if (number < first_number || number > total_commands)
        return NULL;

    return command_history[number - first_number];
}

/* Execute one ordinary command line */
int run_command_line(const char *line) {

    /* Handle variable assignment first */
    int assignment_status = handle_variable_assignment(line);

    if (assignment_status != -1)
        return assignment_status;

    /* Expand variables before tokenization */
    char *expanded = expand_variables(line);

    if (expanded == NULL) {
        perror("Variable expansion");
        return 1;
    }

    char **arglist = tokenize(expanded);

    if (arglist == NULL) {
        free(expanded);
        return 0;
    }

    int status;

    if (strcmp(arglist[0], "history") == 0) {
        show_history();
        status = 0;
    }
    else if (strcmp(arglist[0], "set") == 0) {
        show_variables();
        status = 0;
    }
    else {
        status = execute(arglist);
    }

    for (int i = 0; arglist[i] != NULL; i++)
        free(arglist[i]);

    free(arglist);
    free(expanded);

    return status;
}

/* Process multi-line if-then-else-fi blocks */
int process_if_block(const char *if_line) {
    char *condition;
    char *then_lines[MAX_BLOCK_LINES];
    char *else_lines[MAX_BLOCK_LINES];

    int then_count = 0;
    int else_count = 0;
    int in_else = 0;
    int found_fi = 0;

    const char *start = if_line + 2;

    while (*start == ' ' || *start == '\t')
        start++;

    if (*start == '\0') {
        fprintf(stderr, "Missing condition after if.\n");
        return 1;
    }

    condition = strdup(start);

    if (condition == NULL) {
        perror("strdup");
        return 1;
    }

    char *line = read_cmd(PROMPT, stdin);

    if (line == NULL || strcmp(line, "then") != 0) {
        fprintf(stderr, "Expected 'then'.\n");
        free(line);
        free(condition);
        return 1;
    }

    store_history(line);
    add_history(line);
    free(line);

    while ((line = read_cmd(PROMPT, stdin)) != NULL) {
        store_history(line);
        add_history(line);

        if (strcmp(line, "else") == 0) {
            if (in_else) {
                fprintf(stderr, "Unexpected second 'else'.\n");
                free(line);
                break;
            }

            in_else = 1;
            free(line);
            continue;
        }

        if (strcmp(line, "fi") == 0) {
            found_fi = 1;
            free(line);
            break;
        }

        if (in_else) {
            if (else_count >= MAX_BLOCK_LINES) {
                fprintf(stderr, "Else block is too large.\n");
                free(line);
                break;
            }

            else_lines[else_count++] = line;
        } else {
            if (then_count >= MAX_BLOCK_LINES) {
                fprintf(stderr, "Then block is too large.\n");
                free(line);
                break;
            }

            then_lines[then_count++] = line;
        }
    }

    if (!found_fi) {
        fprintf(stderr, "Conditional block must end with 'fi'.\n");

        for (int i = 0; i < then_count; i++)
            free(then_lines[i]);

        for (int i = 0; i < else_count; i++)
            free(else_lines[i]);

        free(condition);
        return 1;
    }

    int condition_status = run_command_line(condition);
    free(condition);

    int result = condition_status;

    if (condition_status == 0) {
        for (int i = 0; i < then_count; i++) {
            result = run_command_line(then_lines[i]);
            free(then_lines[i]);
        }

        for (int i = 0; i < else_count; i++)
            free(else_lines[i]);
    } else {
        for (int i = 0; i < then_count; i++)
            free(then_lines[i]);

        for (int i = 0; i < else_count; i++) {
            result = run_command_line(else_lines[i]);
            free(else_lines[i]);
        }
    }

    return result;
}

/* Main shell loop */
int main(void) {
    char *cmdline;

    using_history();
    stifle_history(HISTORY_SIZE);

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
        if (cmdline[0] == '\0') {
            free(cmdline);
            continue;
        }

        /* Expand !n history commands */
        if (cmdline[0] == '!') {
            char *end;
            long number = strtol(cmdline + 1, &end, 10);

            if (cmdline[1] == '\0' || *end != '\0' || number <= 0) {
                printf("Invalid history command.\n");
                free(cmdline);
                continue;
            }

            char *previous = get_history((int)number);

            if (previous == NULL) {
                printf("History command not found.\n");
                free(cmdline);
                continue;
            }

            char *expanded_history = strdup(previous);

            if (expanded_history == NULL) {
                perror("strdup");
                free(cmdline);
                continue;
            }

            free(cmdline);
            cmdline = expanded_history;

            printf("%s\n", cmdline);
        }

        store_history(cmdline);
        add_history(cmdline);

        if (strncmp(cmdline, "if ", 3) == 0 ||
            strcmp(cmdline, "if") == 0) {
            process_if_block(cmdline);
        } else {
            run_command_line(cmdline);
        }

        free(cmdline);
    }

    printf("\nShell exited.\n");
    clear_history();

    return 0;
}
