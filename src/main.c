
#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

#define HISTORY_SIZE 20

/* Custom command history */
char command_history[HISTORY_SIZE][MAX_LEN];
int history_count = 0;
int total_commands = 0;

/* Store a complete command in custom history */
void store_history(const char *command) {
    if (history_count == HISTORY_SIZE) {
        memmove(command_history,
                command_history + 1,
                sizeof(command_history[0]) * (HISTORY_SIZE - 1));
    } else {
        history_count++;
    }

    snprintf(command_history[history_count - 1],
             MAX_LEN, "%s", command);

    total_commands++;
}

/* Display custom command history */
void show_history(void) {
    int first_number = total_commands - history_count + 1;

    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n",
               first_number + i,
               command_history[i]);
    }
}

/* Retrieve command by history number */
char *get_history(int number) {
    int first_number = total_commands - history_count + 1;

    if (number < first_number || number > total_commands) {
        return NULL;
    }

    return command_history[number - first_number];
}

int main(void) {
    char *cmdline;
    char **arglist;

    /* Initialize GNU Readline history */
    using_history();
    stifle_history(HISTORY_SIZE);

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {

        /* Handle !n command */
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

            /* Copy the previous command safely */
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

        /* Save the complete command BEFORE tokenization */
        if (cmdline[0] != '\0') {
            store_history(cmdline);
            add_history(cmdline);
        }

        /* Tokenize the command */
        arglist = tokenize(cmdline);

        if (arglist != NULL) {

            /* Execute history built-in */
            if (strcmp(arglist[0], "history") == 0) {
                show_history();
            } else {
                execute(arglist);
            }

            /* Free allocated arguments */
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }

            free(arglist);
        }

        free(cmdline);
    }

    printf("\nShell exited.\n");

    clear_history();

    return 0;
}
