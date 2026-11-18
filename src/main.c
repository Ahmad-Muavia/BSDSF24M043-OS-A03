
#include "shell.h"

#define HISTORY_SIZE 20

char history_list[HISTORY_SIZE][MAX_LEN];
int history_count = 0;
int total_commands = 0;

// Store a command in history
void add_history(char* command) {

    if (history_count == HISTORY_SIZE) {
        // Remove the oldest command
        memmove(history_list, history_list + 1,
                sizeof(history_list[0]) * (HISTORY_SIZE - 1));
    } else {
        history_count++;
    }

    snprintf(history_list[history_count - 1], MAX_LEN, "%s", command);
    total_commands++;
}

// Display stored commands
void show_history() {

    int first_number = total_commands - history_count + 1;

    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", first_number + i, history_list[i]);
    }
}

// Retrieve a command using its history number
char* get_history(int number) {

    int first_number = total_commands - history_count + 1;

    if (number < first_number || number > total_commands) {
        return NULL;
    }

    return history_list[number - first_number];
}

int main() {

    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {

        // Handle !n command
        if (cmdline[0] == '!') {

            char* end;
            long number = strtol(cmdline + 1, &end, 10);

            if (cmdline[1] == '\0' || *end != '\0' || number <= 0) {
                printf("Invalid history command.\n");
                free(cmdline);
                continue;
            }

            char* previous = get_history((int)number);

            if (previous == NULL) {
                printf("History command not found.\n");
                free(cmdline);
                continue;
            }

            char temp[MAX_LEN];
            snprintf(temp, MAX_LEN, "%s", previous);
            snprintf(cmdline, MAX_LEN, "%s", temp);

            printf("%s\n", cmdline);
        }

        if ((arglist = tokenize(cmdline)) != NULL) {

            add_history(cmdline);

            // Handle history built-in
            if (strcmp(arglist[0], "history") == 0) {
                show_history();
            } else {
                execute(arglist);
            }

            // Free allocated argument memory
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }

            free(arglist);
        }

        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}

