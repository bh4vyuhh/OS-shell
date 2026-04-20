#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>


/* ---------- Function Declarations ---------- */
void shell_loop(void);
char *read_line(void);
char **split_line(char *line);
int execute(char **args);

/* Builtins */
int shell_cd(char **args);
int shell_help(char **args);
int shell_exit(char **args);

/* ---------- Builtin Commands ---------- */
char *builtin_str[] = {
    "cd",
    "help",
    "exit"
};

int (*builtin_func[]) (char **) = {
    &shell_cd,
    &shell_help,
    &shell_exit
};

int num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

/* ---------- Builtin Implementations ---------- */

int shell_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "Expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd failed");
        }
    }
    return 1;
}

int shell_help(char **args) {
    printf("Simple Shell\n");
    printf("Built-in commands:\n");

    for (int i = 0; i < num_builtins(); i++) {
        printf("  %s\n", builtin_str[i]);
    }

    printf("Use man command for external programs.\n");
    return 1;
}

int shell_exit(char **args) {
    return 0;
}

/* ---------- Launch External Commands ---------- */

int launch(char **args) {
    pid_t pid1, pid2;
    int status;

    int i;
    int pipe_pos = -1;
    char *infile = NULL;
    char *outfile = NULL;

    // Detect pipe and redirection
    for (i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_pos = i;
        } else if (strcmp(args[i], ">") == 0) {
            outfile = args[i + 1];
            args[i] = NULL;
        } else if (strcmp(args[i], "<") == 0) {
            infile = args[i + 1];
            args[i] = NULL;
        }
    }

    // -------- PIPE CASE --------
    if (pipe_pos != -1) {
        args[pipe_pos] = NULL;
        char **args2 = &args[pipe_pos + 1];

        int pipefd[2];
        pipe(pipefd);

        pid1 = fork();
        if (pid1 == 0) {
            // First command (left side)
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);

            if (execvp(args[0], args) == -1) {
                perror("pipe cmd1");
            }
            exit(EXIT_FAILURE);
        }

        pid2 = fork();
        if (pid2 == 0) {
            // Second command (right side)
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[1]);
            close(pipefd[0]);

            if (execvp(args2[0], args2) == -1) {
                perror("pipe cmd2");
            }
            exit(EXIT_FAILURE);
        }

        close(pipefd[0]);
        close(pipefd[1]);

        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);

        return 1;
    }

    // -------- NORMAL / REDIRECTION --------
    pid1 = fork();

    if (pid1 == 0) {
        // Input redirection
        if (infile != NULL) {
            int fd = open(infile, O_RDONLY);
            if (fd < 0) {
                perror("input file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // Output redirection
        if (outfile != NULL) {
            int fd = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd < 0) {
                perror("output file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (execvp(args[0], args) == -1) {
            perror("shell");
        }
        exit(EXIT_FAILURE);

    } else if (pid1 < 0) {
        perror("fork failed");
    } else {
        waitpid(pid1, &status, 0);
    }

    return 1;
}

/* ---------- Execute Command ---------- */

int execute(char **args) {
    if (args[0] == NULL) {
        return 1;
    }

    for (int i = 0; i < num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return launch(args);
}

/* ---------- Read Line ---------- */

#define BUFFER_SIZE 1024

char *read_line(void) {
    char *line = readline("myshell> ");

    if (!line) {
        exit(EXIT_SUCCESS);
    }

    if (strlen(line) > 0) {
        add_history(line);
    }

    return line;
}

/* ---------- Split Line ---------- */

#define TOKEN_BUFFER 64
#define TOKEN_DELIM " \t\r\n"

char **split_line(char *line) {
    int bufsize = TOKEN_BUFFER, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, TOKEN_DELIM);
    while (token != NULL) {
        tokens[position++] = token;

        if (position >= bufsize) {
            bufsize += TOKEN_BUFFER;
            tokens = realloc(tokens, bufsize * sizeof(char*));

            if (!tokens) {
                fprintf(stderr, "Allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, TOKEN_DELIM);
    }

    tokens[position] = NULL;
    return tokens;
}

/* ---------- Shell Loop ---------- */

void shell_loop(void) {
    char *line;
    char **args;
    int status;

    do {
        line = read_line();
        args = split_line(line);
        status = execute(args);

        free(line);
        free(args);
    } while (status);
}

/* ---------- Main ---------- */

int main(int argc, char **argv) {
    shell_loop();
    return EXIT_SUCCESS;
}
