#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

#define HISTORY_SIZE 20
#define MAX_BLOCK_LINES 100

char command_history[HISTORY_SIZE][MAX_LEN];
int history_count = 0;
int total_commands = 0;

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
        printf("%d  %s\n", first_number + i, command_history[i]);
    }
}

char *get_history(int number) {
    int first_number = total_commands - history_count + 1;

    if (number < first_number || number > total_commands)
        return NULL;

    return command_history[number - first_number];
}

int run_command_line(const char *line) {
    char *copy = strdup(line);

    if (copy == NULL) {
        perror("strdup");
        return 1;
    }

    char **arglist = tokenize(copy);

    if (arglist == NULL) {
        free(copy);
        return 0;
    }

    int status;

    if (strcmp(arglist[0], "history") == 0) {
        show_history();
        status = 0;
    } else {
        status = execute(arglist);
    }

    for (int i = 0; arglist[i] != NULL; i++)
        free(arglist[i]);

    free(arglist);
    free(copy);

    return status;
}

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

int main(void) {
    char *cmdline;

    using_history();
    stifle_history(HISTORY_SIZE);

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
        if (cmdline[0] == '\0') {
            free(cmdline);
            continue;
        }

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

            char *expanded = strdup(previous);

            if (expanded == NULL) {
                perror("strdup");
                free(cmdline);
                continue;
            }

            free(cmdline);
            cmdline = expanded;
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
