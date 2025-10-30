// CP.c  -- full featured student shell (builtins, history, pipes, redirection, background)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>
#define MAX_INPUT_SIZE 1024

#define MAX_INPUT 4096
#define MAX_TOKENS 256
#define MAX_COMMANDS 64
#define HISTORY_FILE ".myshell_history"
#define MAX_HISTORY 1000

/* ---------------- Global history ---------------- */
char *history[MAX_HISTORY];
int history_count = 0;

/* ---------------- Function declarations ---------------- */
void load_history();
void save_history();
void add_history(const char *line);

void print_prompt();
char *read_input();
char **tokenize(char *line, const char *delim);
int is_builtin(const char *cmd);
int handle_builtin_parent(char **args);    // run builtin in parent (no fork)
int handle_builtin_child(char **args);     // run builtin inside child (for pipelines)
void free_tokens(char **tokens);

int execute_line(char *line);
int execute_simple_command(char **args, int in_fd, int out_fd, int is_background);
int execute_pipeline(char *line, int is_background);

void trim(char *s);

/* ---------------- Implementation ---------------- */

void load_history() {
    FILE *f = fopen(HISTORY_FILE, "r");
    if (!f) return;
    char *line = NULL;
    size_t n = 0;
    while (getline(&line, &n, f) != -1) {
        line[strcspn(line, "\n")] = 0;
        if (history_count < MAX_HISTORY) history[history_count++] = strdup(line);
    }
    free(line);
    fclose(f);
}

void save_history() {
    FILE *f = fopen(HISTORY_FILE, "w");
    if (!f) return;
    for (int i = 0; i < history_count; ++i) {
        fprintf(f, "%s\n", history[i]);
    }
    fclose(f);
}

void add_history(const char *line) {
    if (!line || line[0] == '\0') return;
    if (history_count >= MAX_HISTORY) {
        free(history[0]);
        memmove(&history[0], &history[1], sizeof(char*) * (MAX_HISTORY - 1));
        history_count--;
    }
    history[history_count++] = strdup(line);
}

/* Print prompt */
void print_prompt() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("myshell:%s> ", cwd);
    } else {
        printf("myshell?> ");
    }
    fflush(stdout);
}

/* Read a line from stdin */

char *read_input() {
    static char buffer[MAX_INPUT_SIZE];

    printf("myshell> ");
    if (fgets(buffer, MAX_INPUT_SIZE, stdin) == NULL) {
        printf("\n");
        return NULL;
    }

    // Remove trailing newline, if present
    buffer[strcspn(buffer, "\n")] = '\0';
    return buffer;
}

/* Trim leading/trailing spaces */
void trim(char *s) {
    // leading
    char *p = s;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    // trailing
    int L = strlen(s);
    while (L > 0 && (s[L - 1] == ' ' || s[L - 1] == '\t')) s[--L] = 0;
}

/* Tokenize a string (destructive to line) */
char **tokenize(char *line, const char *delim) {
    char **tokens = calloc(MAX_TOKENS, sizeof(char*));
    char *tok;
    int pos = 0;
    tok = strtok(line, delim);
    while (tok != NULL && pos < MAX_TOKENS - 1) {
        tokens[pos++] = strdup(tok);
        tok = strtok(NULL, delim);
    }
    tokens[pos] = NULL;
    return tokens;
}
void free_tokens(char **tokens) {
    if (!tokens) return;
    for (int i = 0; tokens[i] != NULL; ++i) free(tokens[i]);
    free(tokens);
}

/* Builtin detection */
int is_builtin(const char *cmd) {
    if (!cmd) return 0;
    return (strcmp(cmd, "cd") == 0 ||
            strcmp(cmd, "pwd") == 0 ||
            strcmp(cmd, "echo") == 0 ||
            strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "history") == 0);
}

int handle_builtin_parent(char **args) {
    if (!args[0]) return 0;

    if (strcmp(args[0], "cd") == 0) {
        char *target_dir = args[1];
        if (!target_dir || strcmp(target_dir, "~") == 0) {
            target_dir = getenv("HOME");
            if (!target_dir) target_dir = "/";
        }
        if (chdir(target_dir) != 0) perror("cd");
        return 1;
    }

    if (strcmp(args[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
        else perror("pwd");
        return 1;
    }

    if (strcmp(args[0], "echo") == 0) {
        for (int i = 1; args[i]; ++i) {
            printf("%s", args[i]);
            if (args[i+1]) printf(" ");
        }
        printf("\n");
        return 1;
    }

    if (strcmp(args[0], "history") == 0) {
        for (int i = 0; i < history_count; ++i)
            printf("%d %s\n", i+1, history[i]);
        return 1;
    }

    if (strcmp(args[0], "exit") == 0) {
        return 0;
    }

    return 0;
}


/* Builtins executed in child (when inside pipeline). Return 1 if handled. */
int handle_builtin_child(char **args) {
    if (!args[0]) return 0;
    if (strcmp(args[0], "cd") == 0) {
        if (!args[1]) exit(EXIT_FAILURE);
        if (chdir(args[1]) != 0) { perror("cd"); exit(EXIT_FAILURE); }
        exit(EXIT_SUCCESS);
    }
    if (strcmp(args[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
        else perror("pwd");
        exit(EXIT_SUCCESS);
    }
    if (strcmp(args[0], "echo") == 0) {
        for (int i = 1; args[i]; ++i) {
            printf("%s", args[i]);
            if (args[i+1]) printf(" ");
        }
        printf("\n");
        exit(EXIT_SUCCESS);
    }
    if (strcmp(args[0], "history") == 0) {
        for (int i = 0; i < history_count; ++i) {
            printf("%d %s\n", i+1, history[i]);
        }
        exit(EXIT_SUCCESS);
    }
    return 0;
}

/* Execute a simple command (no pipes). in_fd/out_fd allow redirection; -1 means use default.
   Returns 0 normally, 2 on exit request. */
int execute_simple_command(char **args, int in_fd, int out_fd, int is_background) {
    if (!args[0]) return 0;

    // --- Handle builtins in parent process ---
    if (is_builtin(args[0])) {
        int saved_stdout = dup(STDOUT_FILENO);  // Save current stdout

        // If there's output redirection, duplicate out_fd to stdout
        if (out_fd != STDOUT_FILENO) {
            dup2(out_fd, STDOUT_FILENO);
        }

        // If there's input redirection, duplicate in_fd to stdin
        int saved_stdin = dup(STDIN_FILENO);
        if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
        }

        // Execute builtin
        int handled = handle_builtin_parent(args);

        // Restore standard I/O
        dup2(saved_stdout, STDOUT_FILENO);
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdout);
        close(saved_stdin);

        return handled;
    }

    // --- Handle external commands ---
    pid_t pid = fork();
    if (pid == 0) {
        if (in_fd != STDIN_FILENO) dup2(in_fd, STDIN_FILENO);
        if (out_fd != STDOUT_FILENO) dup2(out_fd, STDOUT_FILENO);
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else {
        if (!is_background) waitpid(pid, NULL, 0);
    }

    return 0;
}



int handle_redirection_in_tokens(char **tokens, int *in_fd, int *out_fd, int *append_flag) {
    *in_fd = -1;
    *out_fd = -1;
    *append_flag = 0;
    for (int i = 0; tokens[i]; ++i) {
        if (strcmp(tokens[i], "<") == 0) {
            if (!tokens[i+1]) return -1;
            int fd = open(tokens[i+1], O_RDONLY);
            if (fd < 0) { perror("open"); return -1; }
            *in_fd = fd;
            free(tokens[i]); free(tokens[i+1]);
            // shift tokens left
            int j = i;
            while (tokens[j+2]) { tokens[j] = tokens[j+2]; j++; }
            tokens[j] = NULL;
            tokens[j+1] = NULL;
            i--; // re-evaluate current index
        } else if (strcmp(tokens[i], ">") == 0 || strcmp(tokens[i], ">>") == 0) {
            if (!tokens[i+1]) return -1;
            int flags = O_WRONLY | O_CREAT;
            if (strcmp(tokens[i], ">>") == 0) { flags |= O_APPEND; *append_flag = 1; }
            else { flags |= O_TRUNC; *append_flag = 0; }
            int fd = open(tokens[i+1], flags, 0644);
            if (fd < 0) { perror("open"); return -1; }
            *out_fd = fd;
            free(tokens[i]); free(tokens[i+1]);
            int j = i;
            while (tokens[j+2]) { tokens[j] = tokens[j+2]; j++; }
            tokens[j] = NULL;
            tokens[j+1] = NULL;
            i--;
        }
    }
    return 0;
}

/* Execute a pipeline line. Returns 0 normally, 2 on exit request */
int execute_pipeline(char *line, int is_background) {
    // Split by '|'
    char *cmds[MAX_COMMANDS];
    int cmd_count = 0;
    char *saveptr = NULL;
    char *sline = strdup(line);
    char *part = strtok_r(sline, "|", &saveptr);
    while (part && cmd_count < MAX_COMMANDS - 1) {
        trim(part);
        cmds[cmd_count++] = strdup(part);
        part = strtok_r(NULL, "|", &saveptr);
    }
    cmds[cmd_count] = NULL;

    if (cmd_count == 0) { free(sline); return 0; }

    // Special-case: single command -> use execute_simple_command with redir parsing
    if (cmd_count == 1) {
        char *c = strdup(cmds[0]);
        char **tokens = tokenize(c, " \t\n");
        // detect background & remove token
        int background = is_background;
        int t = 0;
        while (tokens[t]) t++;
        if (t > 0 && strcmp(tokens[t-1], "&") == 0) { background = 1; free(tokens[t-1]); tokens[t-1]=NULL; }

        int in_fd = -1, out_fd = -1, append = 0;
        if (handle_redirection_in_tokens(tokens, &in_fd, &out_fd, &append) < 0) {
            fprintf(stderr, "Redirection syntax error\n");
            free_tokens(tokens); free(c);
            free(sline);
            for (int i=0;i<cmd_count;i++) free(cmds[i]);
            return 0;
        }
        int rc = execute_simple_command(tokens, in_fd, out_fd, background);
        if (in_fd != -1) close(in_fd);
        if (out_fd != -1) close(out_fd);
        free_tokens(tokens); free(c);
        free(sline); for (int i=0;i<cmd_count;i++) free(cmds[i]);
        if (rc == 2) return 2;
        return 0;
    }

    // For multiple commands -> set up pipes
    int num_pipes = cmd_count - 1;
    int pipefds[2 * num_pipes];
    for (int i = 0; i < num_pipes; ++i) {
        if (pipe(pipefds + i*2) < 0) { perror("pipe"); return 0; }
    }

    for (int i = 0; i < cmd_count; ++i) {
        char *c = strdup(cmds[i]);
        char **tokens = tokenize(c, " \t\n");

        // check background token on last command only
        if (i == cmd_count - 1) {
            int t = 0; while (tokens[t]) t++;
            if (t > 0 && strcmp(tokens[t-1], "&") == 0) {
                is_background = 1;
                free(tokens[t-1]); tokens[t-1] = NULL;
            }
        }

        int in_fd = -1, out_fd = -1, append = 0;
        if (handle_redirection_in_tokens(tokens, &in_fd, &out_fd, &append) < 0) {
            fprintf(stderr, "Redirection syntax error\n");
            free_tokens(tokens); free(c);
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            // child
            // if not first, connect read end of previous pipe to stdin
            if (i != 0) {
                dup2(pipefds[(i-1)*2], STDIN_FILENO);
            } else if (in_fd != -1) {
                dup2(in_fd, STDIN_FILENO);
            }

            // if not last, connect stdout to write end of current pipe
            if (i != cmd_count - 1) {
                dup2(pipefds[i*2 + 1], STDOUT_FILENO);
            } else if (out_fd != -1) {
                dup2(out_fd, STDOUT_FILENO);
            }

            // close all pipe fds in child
            for (int k = 0; k < 2*num_pipes; ++k) close(pipefds[k]);
            if (in_fd != -1) close(in_fd);
            if (out_fd != -1) close(out_fd);

            // If builtin inside pipeline, run in child
            if (is_builtin(tokens[0])) {
                if (handle_builtin_child(tokens)) exit(EXIT_SUCCESS);
            }

            execvp(tokens[0], tokens);
            fprintf(stderr, "Command failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            perror("fork");
        } else {
            // parent continues, close fds that are not needed
            if (in_fd != -1) close(in_fd);
            if (out_fd != -1) close(out_fd);
        }

        free_tokens(tokens);
        free(c);
    }

    // parent closes all pipe fds
    for (int k = 0; k < 2*num_pipes; ++k) close(pipefds[k]);

    // wait for children if not background
    if (!is_background) {
        for (int i = 0; i < cmd_count; ++i) wait(NULL);
    } else {
        printf("[Background] pipeline launched\n");
    }

    free(sline);
    for (int i=0;i<cmd_count;i++) free(cmds[i]);
    return 0;
}

/* Execute a line (handles history commands !n, and top-level parsing) */
int execute_line(char *line) {
    if (!line) return 0;
    trim(line);
    if (line[0] == 0) return 0;

    // history expansion: !! or !n
    if (line[0] == '!') {
        if (line[1] == '!') {
            if (history_count == 0) { printf("No history\n"); return 0; }
            free(line);
            line = strdup(history[history_count - 1]);
            printf("%s\n", line);
        } else {
            int idx = atoi(line + 1) - 1;
            if (idx < 0 || idx >= history_count) { printf("No such command in history\n"); return 0; }
            free(line);
            line = strdup(history[idx]);
            printf("%s\n", line);
        }
    }

    // add to history and persist
    add_history(line);
    save_history();

    // detect background overall (if trailing & not in pipeline)
    int is_background = 0;
    int L = strlen(line);
    if (L > 0 && line[L - 1] == '&') {
        is_background = 1;
        // but pipeline handler also checks last command
    }

    // if contains pipe
    if (strchr(line, '|') != NULL) {
        int rc = execute_pipeline(line, is_background);
        return rc == 2 ? 2 : 0;
    } else {
        // single command - parse tokens, handle redirection
        char *copy = strdup(line);
        char **tokens = tokenize(copy, " \t\n");

        // if empty
        if (!tokens[0]) { free_tokens(tokens); free(copy); return 0; }

        // exit special-case
        if (strcmp(tokens[0], "exit") == 0) {
            free_tokens(tokens); free(copy);
            return 2;
        }

        // detect background at end
        int t = 0; while (tokens[t]) t++;
        if (t > 0 && strcmp(tokens[t-1], "&") == 0) {
            is_background = 1;
            free(tokens[t-1]); tokens[t-1] = NULL;
        }

        int in_fd = -1, out_fd = -1, append = 0;
        if (handle_redirection_in_tokens(tokens, &in_fd, &out_fd, &append) < 0) {
            fprintf(stderr, "Redirection syntax error\n");
            free_tokens(tokens); free(copy);
            return 0;
        }

        int rc = execute_simple_command(tokens, in_fd, out_fd, is_background);
        if (in_fd != -1) close(in_fd);
        if (out_fd != -1) close(out_fd);
        free_tokens(tokens); free(copy);
        return rc == 2 ? 2 : 0;
    }
}

/* ---------------- Main loop ---------------- */
int main_loop() {
    load_history();

    // ASCII banner
    system("figlet 'Welcome to My_Shell !!'");
    printf("\n");

    while (1) {
        print_prompt();

        char *line = read_input();
        if (!line)
            break;

        // skip empty input
        if (strlen(line) == 0)
            continue;

        // duplicate the input line before storing in history
        add_history(strdup(line));

        int rc = execute_line(line);

        if (rc == 2)  // exit
            break;
    }

    save_history();

    // free history memory
    for (int i = 0; i < history_count; i++)
        free(history[i]);

    printf("Exiting MyShell...\n");
    return 0;
}


int main(int argc, char **argv) {
    // If already inside the new terminal (child process)
    if (argc > 1 && strcmp(argv[1], "--child") == 0) {
        return main_loop();
    }

    // Desktop GUI terminals
    if (system("which gnome-terminal > /dev/null 2>&1") == 0) {
        system("gnome-terminal -- bash -c './myshell --child; exec bash'");
        exit(0);
    } else if (system("which xterm > /dev/null 2>&1") == 0) {
        system("xterm -hold -e ./myshell --child");
        exit(0);
    } else if (system("which konsole > /dev/null 2>&1") == 0) {
        system("konsole -e ./myshell --child");
        exit(0);
    }

    // Docker or VSCode fallback – use pseudo-terminal
    else if (system("which script > /dev/null 2>&1") == 0) {
        system("script -q -c './myshell --child' /dev/null");
        exit(0);
    } else if (system("which tmux > /dev/null 2>&1") == 0) {
        system("tmux new-session ./myshell --child");
        exit(0);
    } else if (system("which screen > /dev/null 2>&1") == 0) {
        system("screen ./myshell --child");
        exit(0);
    }

    printf("⚠️ No compatible terminal emulator found — running inline.\n");
    return main_loop();
}
