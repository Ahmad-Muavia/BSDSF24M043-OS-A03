
#include "shell.h"
#include <fcntl.h>

#define MAX_JOBS 100

/* Store background process IDs */
static pid_t background_jobs[MAX_JOBS];
static int job_count = 0;

/* Remove completed background processes */
void reap_background_jobs(void) {
    for (int i = 0; i < job_count;) {
        int status;

        pid_t result = waitpid(background_jobs[i], &status, WNOHANG);

        if (result > 0 || (result == -1 && errno == ECHILD)) {
            printf("[Job completed] PID: %d\n",
                   (int)background_jobs[i]);

            for (int j = i; j < job_count - 1; j++) {
                background_jobs[j] = background_jobs[j + 1];
            }

            job_count--;
        } else {
            i++;
        }
    }
}

/* Add a process to the background job list */
void add_background_job(pid_t pid) {
    if (job_count < MAX_JOBS) {
        background_jobs[job_count++] = pid;
        printf("[Background] PID: %d\n", (int)pid);
    } else {
        fprintf(stderr, "Job list full. Waiting for PID %d.\n",
                (int)pid);

        int status;
        waitpid(pid, &status, 0);
    }
}

/* Display background jobs */
void show_jobs(void) {
    reap_background_jobs();

    if (job_count == 0) {
        printf("No background jobs.\n");
        return;
    }

    printf("Background jobs:\n");

    for (int i = 0; i < job_count; i++) {
        printf("[%d] PID: %d\n",
               i + 1, (int)background_jobs[i]);
    }
}

/*
 * Handle built-in commands.
 * Return:
 *   -1 = not a built-in
 *    0 = success
 *    1 = failure
 */
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
            return 1;
        }

        return 0;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Available commands:\n");
        printf("exit - Exit the shell\n");
        printf("cd - Change directory\n");
        printf("help - Display available commands\n");
        printf("jobs - Display background jobs\n");
        return 0;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        show_jobs();
        return 0;
    }

    return -1;
}

/* Convert a child process status into a shell exit status */
static int get_exit_status(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return 1;
}

/* Execute one command or pipeline */
static int execute_pipeline(char **arglist, int background) {
    char *commands[MAXARGS][MAXARGS + 1] = {{0}};
    char *input_files[MAXARGS] = {NULL};
    char *output_files[MAXARGS] = {NULL};

    int stage = 0;
    int arg_index = 0;

    /* Parse pipes and redirections */
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            if (arg_index == 0 || stage >= MAXARGS - 1) {
                fprintf(stderr, "Invalid pipe syntax.\n");
                return 1;
            }

            commands[stage][arg_index] = NULL;
            stage++;
            arg_index = 0;
        }
        else if (strcmp(arglist[i], "<") == 0) {
            if (arglist[i + 1] == NULL) {
                fprintf(stderr, "Input filename missing.\n");
                return 1;
            }

            input_files[stage] = arglist[++i];
        }
        else if (strcmp(arglist[i], ">") == 0) {
            if (arglist[i + 1] == NULL) {
                fprintf(stderr, "Output filename missing.\n");
                return 1;
            }

            output_files[stage] = arglist[++i];
        }
        else {
            if (arg_index >= MAXARGS) {
                fprintf(stderr, "Too many arguments.\n");
                return 1;
            }

            commands[stage][arg_index++] = arglist[i];
        }
    }

    commands[stage][arg_index] = NULL;

    int command_count = stage + 1;

    /* Validate pipeline commands */
    for (int i = 0; i < command_count; i++) {
        if (commands[i][0] == NULL) {
            fprintf(stderr, "Empty command in pipeline.\n");
            return 1;
        }
    }

    /* Execute simple built-ins in the shell process */
    if (command_count == 1 &&
        input_files[0] == NULL &&
        output_files[0] == NULL &&
        !background) {

        int builtin_status = handle_builtin(commands[0]);

        if (builtin_status != -1) {
            return builtin_status;
        }
    }

    pid_t pids[MAXARGS];
    int started = 0;
    int previous_read = -1;

    /* Start each pipeline stage */
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
            /* Connect previous pipeline output to input */
            if (previous_read != -1) {
                if (dup2(previous_read, STDIN_FILENO) == -1) {
                    perror("dup2 input failed");
                    _exit(1);
                }
            }

            /* Connect output to the next pipeline stage */
            if (pipefd[1] != -1) {
                if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                    perror("dup2 output failed");
                    _exit(1);
                }
            }

            if (previous_read != -1) close(previous_read);
            if (pipefd[0] != -1) close(pipefd[0]);
            if (pipefd[1] != -1) close(pipefd[1]);

            /* Input redirection */
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

            /* Output redirection */
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
            _exit(127);
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

    /* Handle background execution */
    if (background) {
        for (int i = 0; i < started; i++) {
            add_background_job(pids[i]);
        }

        return 0;
    }

    /* Wait for foreground processes */
    int final_status = 1;

    for (int i = 0; i < started; i++) {
        int status;
        pid_t result;

        do {
            result = waitpid(pids[i], &status, 0);
        } while (result == -1 && errno == EINTR);

        if (result == -1) {
            perror("waitpid");
            final_status = 1;
        } else if (i == started - 1) {
            final_status = get_exit_status(status);
        }
    }

    return final_status;
}

/* Execute chained commands and background commands */
int execute(char *arglist[]) {
    if (arglist == NULL || arglist[0] == NULL) {
        return 0;
    }

    reap_background_jobs();

    char *command[MAXARGS + 1];
    int command_length = 0;
    int final_status = 0;

    for (int i = 0; ; i++) {
        char *token = arglist[i];

        if (token == NULL ||
            strcmp(token, ";") == 0 ||
            strcmp(token, "&") == 0) {

            if (command_length > 0) {
                command[command_length] = NULL;

                int background =
                    (token != NULL && strcmp(token, "&") == 0);

                final_status =
                    execute_pipeline(command, background);

                command_length = 0;
            }

            if (token == NULL) {
                break;
            }
        }
        else {
            if (command_length >= MAXARGS) {
                fprintf(stderr, "Too many arguments.\n");
                return 1;
            }

            command[command_length++] = token;
        }
    }

    reap_background_jobs();

    return final_status;
}
