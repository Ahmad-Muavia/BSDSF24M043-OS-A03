#include "shell.h"

int handle_builtin(char** arglist) {

    // exit command
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }

    // cd command
    if (strcmp(arglist[0], "cd") == 0) {
        char* path = arglist[1];

        if (path == NULL) {
            path = getenv("HOME");
        }

        if (chdir(path) != 0) {
            perror("cd failed");
        }

        return 1;
    }

    // help command
    if (strcmp(arglist[0], "help") == 0) {
        printf("Available commands:\n");
        printf("exit - Exit the shell\n");
        printf("cd - Change directory\n");
        printf("help - Display available commands\n");
        printf("jobs - Display background jobs (placeholder)\n");

        return 1;
    }

    // jobs placeholder
    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control is not implemented yet.\n");
        return 1;
    }

    return 0; // Not a built-in command
}

int execute(char* arglist[]) {

    // Check built-in commands before creating a child process
    if (handle_builtin(arglist)) {
        return 0;
    }

    int status;
    int cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);

        case 0:
            execvp(arglist[0], arglist);
            perror("Command not found");
            exit(1);

        default:
            waitpid(cpid, &status, 0);
            return 0;
    }
}
