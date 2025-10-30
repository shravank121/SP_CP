#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>  

#define MAX_INPUT 1024
#define MAX_TOKENS 100
#define MAX_HISTORY 100

// ---------------- Global ----------------
char *history[MAX_HISTORY];
int history_count = 0;

// ---------------- Function declarations ----------------
void print_prompt();
char* read_input();
char** tokenize_input(char* line, const char* delimiter);
int execute_command(char** args);
int handle_builtin(char** args);
void execute_pipeline(char* input);

// ---------------- Main ----------------
int main() {
    char *input;

    printf("Welcome to MyShell! Type 'exit' to quit.\n");

    while (1) {
        print_prompt();

        input = read_input();
        if (strlen(input) == 0) continue;

        // Store in history
        if (history_count < MAX_HISTORY) {
            history[history_count++] = strdup(input);
        }

        // Exit command
        if (strcmp(input, "exit") == 0) {
            printf("Exiting MyShell...\n");
            free(input);
            break;
        }

        // Execute previous command if !n syntax
        if (input[0] == '!') {
            int index = atoi(input + 1) - 1;
            if (index >= 0 && index < history_count) {
                free(input);
                input = strdup(history[index]);
                printf("%s\n", input);
            } else {
                printf("No such command in history\n");
                free(input);
                continue;
            }
        }

        // Check for pipeline
        if (strchr(input, '|') != NULL) {
            execute_pipeline(input);
        } else {
            char **args = tokenize_input(input, " \t\n");
            if (args[0] != NULL) {
                if (!handle_builtin(args)) {
                    execute_command(args);
                }
            }
            free(args);
        }

        free(input);
    }

    // Free history memory
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
    }

    return 0;
}

// ---------------- Prompt ----------------
void print_prompt() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("myshell:%s> ", cwd);
    fflush(stdout);
}

// ---------------- Input Reading ----------------
char* read_input() {
    char *line = NULL;
    size_t bufsize = 0;
    getline(&line, &bufsize, stdin);
    line[strcspn(line, "\n")] = 0; // remove newline
    return line;
}

// ---------------- Tokenization ----------------
char** tokenize_input(char* line, const char* delimiter) {
    char **tokens = malloc(MAX_TOKENS * sizeof(char*));
    char *token;
    int position = 0;

    token = strtok(line, delimiter);
    while (token != NULL && position < MAX_TOKENS - 1) {
        tokens[position++] = token;
        token = strtok(NULL, delimiter);
    }
    tokens[position] = NULL;
    return tokens;
}

// ---------------- Built-in Commands ----------------
int handle_builtin(char** args) {
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "cd: missing argument\n");
        } else {
            if (chdir(args[1]) != 0)
                perror("cd failed");
        }
        return 1;
    }

    if (strcmp(args[0], "pwd") == 0) {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
        return 1;
    }

    if (strcmp(args[0], "echo") == 0) {
        for (int i = 1; args[i] != NULL; i++) {
            printf("%s ", args[i]);
        }
        printf("\n");
        return 1;
    }

    if (strcmp(args[0], "history") == 0) {
        for (int i = 0; i < history_count; i++) {
            printf("%d %s\n", i + 1, history[i]);
        }
        return 1;
    }

    return 0; // Not a built-in
}

// ---------------- External Command Execution ----------------
int execute_command(char** args) {
    int background = 0;
    int i = 0;

    // ---------------- Detect Background Execution ----------------
    while (args[i] != NULL) i++;
    if (i > 0 && strcmp(args[i - 1], "&") == 0) {
        background = 1;
        args[i - 1] = NULL; // remove '&' from args
    }

    pid_t pid = fork();
    if (pid == 0) {
        // ---------------- Handle I/O Redirection ----------------
        for (int j = 0; args[j] != NULL; j++) {
            if (strcmp(args[j], ">") == 0) {
                int fd = open(args[j + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                args[j] = NULL;
                break;
            } else if (strcmp(args[j], "<") == 0) {
                int fd = open(args[j + 1], O_RDONLY);
                if (fd < 0) {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
                args[j] = NULL;
                break;
            }
        }

        // Execute command
        if (execvp(args[0], args) == -1) {
            perror("Command failed");
        }
        exit(EXIT_FAILURE);

    } else if (pid < 0) {
        perror("Fork failed");
    } else {
        // ---------------- Handle Background Wait ----------------
        if (!background) {
            waitpid(pid, NULL, 0);
        } else {
            printf("[Background] PID %d running...\n", pid);
        }
    }
    return 1;
}

// ---------------- Pipeline Execution ----------------
void execute_pipeline(char* input) {
    char *commands[10];
    int n = 0;

    commands[n++] = strtok(input, "|");
    while ((commands[n++] = strtok(NULL, "|")) != NULL);

    int num_pipes = n - 2;
    int pipefds[2 * num_pipes];

    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefds + i * 2) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        char **args = tokenize_input(commands[i], " \t\n");

        pid_t pid = fork();
        if (pid == 0) {
            if (i < num_pipes) {
                dup2(pipefds[j + 1], STDOUT_FILENO);
            }
            if (i != 0) {
                dup2(pipefds[j - 2], STDIN_FILENO);
            }

            for (int k = 0; k < 2 * num_pipes; k++) close(pipefds[k]);

            if (execvp(args[0], args) == -1) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        free(args);
        j += 2;
    }

    for (int i = 0; i < 2 * num_pipes; i++) close(pipefds[i]);
    for (int i = 0; i < num_pipes + 1; i++) wait(NULL);
}
