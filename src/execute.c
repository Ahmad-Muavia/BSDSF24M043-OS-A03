
#include "shell.h"
#include <fcntl.h>

/* Handle built-in commands */
int handle_builtin(char **arglist) {
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        char *path = arglist[1];

        if (path == NULL) {
            path = getenv("HOME");
        }

        if (path == NULL || chdir(path) != 0) {
            perror("cd failed");
        }

        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Available commands:\n");
        printf("exit - Exit the shell\n");
        printf("cd - Change directory\n");
        printf("help - Display available commands\n");
        printf("jobs - Display background jobs (placeholder)\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control is not implemented yet.\n");
        return 1;
    }

    return 0;
}

/* Execute commands with redirection and pipes */
int execute(char *arglist[]) {
    if (arglist == NULL || arglist[0] == NULL) {
        return 0;
    }

    /* Check whether the command contains shell operators */
    int has_operator = 0;

    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0 ||
            strcmp(arglist[i], "<") == 0 ||
            strcmp(arglist[i], ">") == 0) {
            has_operator = 1;
            break;
        }
    }

    /* Execute built-ins directly when there are no operators */
    if (!has_operator && handle_builtin(arglist)) {
        return 0;
    }

    /* Store individual commands and their redirections */
    char *commands[MAXARGS][MAXARGS + 1] = {{0}};
    char *input_files[MAXARGS] = {NULL};
    char *output_files[MAXARGS] = {NULL};

    int stage = 0;
    int arg_index = 0;

    /* Parse the command arguments */
    for (int i = 0; arglist[i] != NULL; i++) {

        if (strcmp(arglist[i], "|") == 0) {
            if (arg_index == 0 || stage >= MAXARGS - 1) {
                fprintf(stderr, "Invalid pipe syntax.\n");
                return -1;
            }

            commands[stage][arg_index] = NULL;
            stage++;
            arg_index = 0;
        }

        else if (strcmp(arglist[i], "<") == 0) {
            if (arglist[i + 1] == NULL) {
                fprintf(stderr, "Input filename missing.\n");
                return -1;
            }

            input_files[stage] = arglist[++i];
        }

        else if (strcmp(arglist[i], ">") == 0) {
            if (arglist[i + 1] == NULL) {
                fprintf(stderr, "Output filename missing.\n");
                return -1;
            }

            output_files[stage] = arglist[++i];
        }

        else {
            if (arg_index >= MAXARGS) {
                fprintf(stderr, "Too many arguments.\n");
                return -1;
            }

            commands[stage][arg_index++] = arglist[i];
        }
    }

    commands[stage][arg_index] = NULL;

    int command_count = stage + 1;

    /* Validate that every pipeline stage has a command */
    for (int i = 0; i < command_count; i++) {
        if (commands[i][0] == NULL) {
            fprintf(stderr, "Empty command in pipeline.\n");
            return -1;
        }
    }

    pid_t pids[MAXARGS];
    int started = 0;
    int previous_read = -1;

    /* Execute each command in the pipeline */
    for (int i = 0; i < command_count; i++) {

        int pipefd[2] = {-1, -1};

        if (i < command_count - 1) {
            if (pipe(pipefd) == -1) {
                perror("pipe failed");
                break;
            }
        }

        pid_t pid = fork();

        if (pid == -1) {
            perror("fork failed");

            if (pipefd[0] != -1) close(pipefd[0]);
            if (pipefd[1] != -1) close(pipefd[1]);

            break;
        }

        if (pid == 0) {

            /* Connect previous command's output to this input */
            if (previous_read != -1) {
                if (dup2(previous_read, STDIN_FILENO) == -1) {
                    perror("dup2 input failed");
                    _exit(1);
                }
            }

            /* Connect this command's output to the next command */
            if (pipefd[1] != -1) {
                if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                    perror("dup2 output failed");
                    _exit(1);
                }
            }

            if (previous_read != -1) close(previous_read);
            if (pipefd[0] != -1) close(pipefd[0]);
            if (pipefd[1] != -1) close(pipefd[1]);

            /* Handle input redirection */
            if (input_files[i] != NULL) {
                int fd = open(input_files[i], O_RDONLY);

                if (fd == -1) {
                    perror("input file");
                    _exit(1);
                }

                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("dup2 input file");
                    close(fd);
                    _exit(1);
                }

                close(fd);
            }

            /* Handle output redirection */
            if (output_files[i] != NULL) {
                int fd = open(output_files[i],
                              O_WRONLY | O_CREAT | O_TRUNC,
                              0644);

                if (fd == -1) {
                    perror("output file");
                    _exit(1);
                }

                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("dup2 output file");
                    close(fd);
                    _exit(1);
                }

                close(fd);
            }

            /* Execute the command */
            execvp(commands[i][0], commands[i]);

            perror("Command execution failed");
            _exit(1);
        }

        /* Parent process */
        pids[started++] = pid;

        if (previous_read != -1) {
            close(previous_read);
        }

        if (pipefd[1] != -1) {
            close(pipefd[1]);
        }

        previous_read = pipefd[0];
    }

    if (previous_read != -1) {
        close(previous_read);
    }

    /* Wait for all successfully started children */
    for (int i = 0; i < started; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }

    return 0;
}
