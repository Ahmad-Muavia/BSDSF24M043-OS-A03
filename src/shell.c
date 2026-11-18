
#include "shell.h"
#include <readline/readline.h>

/* Read a command from the user */
char* read_cmd(char* prompt, FILE* fp) {
    if (fp == stdin) {
        return readline(prompt);
    }

    printf("%s", prompt);

    char* line = malloc(MAX_LEN);

    if (line == NULL) {
        perror("malloc");
        return NULL;
    }

    if (fgets(line, MAX_LEN, fp) == NULL) {
        free(line);
        return NULL;
    }

    line[strcspn(line, "\n")] = '\0';

    return line;
}

/* Split command into arguments */
char** tokenize(char* cmdline) {
    char** arglist = malloc(sizeof(char*) * (MAXARGS + 1));

    if (arglist == NULL) {
        perror("malloc");
        return NULL;
    }

    int argnum = 0;
    char* token = strtok(cmdline, " \t\n");

    while (token != NULL && argnum < MAXARGS) {
        arglist[argnum] = malloc(strlen(token) + 1);

        if (arglist[argnum] == NULL) {
            perror("malloc");

            for (int i = 0; i < argnum; i++) {
                free(arglist[i]);
            }

            free(arglist);
            return NULL;
        }

        strcpy(arglist[argnum], token);
        argnum++;

        token = strtok(NULL, " \t\n");
    }

    arglist[argnum] = NULL;

    if (argnum == 0) {
        free(arglist);
        return NULL;
    }

    return arglist;
}

