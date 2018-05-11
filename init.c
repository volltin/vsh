/*
 * Source code of V Shell
 * Author: @volltin
 * reference:
 * POSIX.1-2017 http://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>

/*
 * Constants
 */
#define VSH_ARGS_BUFF_SIZE (1024 * sizeof(char*))
#define VSH_ARG_BUFF_SIZE (1024 * sizeof(char))
#define VSH_BLANK_CHARS " \t\n\a\r"

/*
 * Prepare rules
 */
#define VSH_PREPARE_RULE_NUM 3
const char *VSH_PREPARE_RULE[VSH_PREPARE_RULE_NUM][2] = {
        {"|", " | "},
        {"<", " < "},
        {">", " > "}
};

/*
 * Redirect symbols
 */
#define VSH_REDIRECT_SYMBOL_NUM 2
const char *VSH_REDIRECT_SYMBOL[VSH_REDIRECT_SYMBOL_NUM] = {
        ">",
        "<"
};

/*
 * Built-in commands
 */
#define VSH_BUILTIN_NUM 5
const char *VSH_BUILTINS[] = {
        "cd",
        "pwd",
        "exit",
        "export",
        "alias"
};

/*
 * Global variables
 */
int last_exit_status = 0;
static bool vsh_is_child = false;

/* Init & Clean up */
void vsh_init();

void vsh_clean();

/* Help text */
void vsh_prompt();

void vsh_error_exit(const char *cause);

/* Commands process */
char *vsh_get_line();

char *vsh_prepare_line(const char *_line);

char **vsh_split_line(const char *line);

char ***vsh_parse_args(char **args);

bool vsh_is_builtin(char **args);

char *vsh_str_replace(const char *str, const char *oldstr, const char *newstr);

/* Execute */
int vsh_pipeline(char ***argss, int cur, int in_fd);

int vsh_execute(char **args);

int vsh_exec_builtin(char **args);

int vsh_run(char **args);

/* Main loop */
void vsh_loop();

/* Memory manage */
void vsh_free(void *p);

/*
 * main entry of the V shell:
 *  - init
 *      load config or do something else
 *  - loop
 *      main loop of the V shell
 *  - clean
 *      do some cleanup work
 */
int vsh_main() {
    
    vsh_init();
    
    vsh_loop();
    
    vsh_clean();
    
    return 0;
}

/* main */

int main() {
    int ret = vsh_main();
    return ret;
}

/*
 * print error and exit
 * if the process is a child, call _exit()
 * else call exit()
 */
void vsh_error_exit(const char *cause) {
    perror(cause);
    if (vsh_is_child) _exit(EXIT_FAILURE);
    else exit(EXIT_FAILURE);
}

/*
 * print prompt
 * based on last exit status code
 */
void vsh_prompt() {
    if (last_exit_status) {
        // red
        printf("\033[31m~ \033[0m");
    } else {
        // green
        printf("\033[32m~ \033[0m");
    }
    fflush(stdout);
    fflush(stdin);
}

/*
 * wrapper for free()
 */
void vsh_free(void *p) {
    free(p);
}

/*
 * get a line from stdin
 */
char *vsh_get_line() {
    char *line = NULL;
    size_t line_size = 0;
    getline(&line, &line_size, stdin);
    line[line_size - 2] = '\0';
    return line;
}

/*
 * replace `oldstr` with `newstr` in `str`
 */
char *vsh_str_replace(const char *str, const char *oldstr, const char *newstr) {
    char *buffer = (char *) malloc(VSH_ARG_BUFF_SIZE);
    char *p;
    
    if (!(p = strstr(str, oldstr))) {
        strncpy(buffer, str, VSH_ARG_BUFF_SIZE);
        return buffer;
    }
    
    strncpy(buffer, str, p - str);
    buffer[p - str] = '\0';
    
    sprintf(buffer + (p - str), "%s%s", newstr, p + strlen(oldstr));
    
    return buffer;
}

char *vsh_prepare_line(const char *_line) {
    char *line, *tmp;
    
    line = strdup(_line);
    
    for (size_t idx = 0; idx < VSH_PREPARE_RULE_NUM; idx++) {
        tmp = vsh_str_replace(line, VSH_PREPARE_RULE[idx][0], VSH_PREPARE_RULE[idx][1]);
        vsh_free(line);
        line = tmp;
    }
    
    return line;
}

/*
 * do some preparing work:
 *  replace "|" with " | " for convenience
 * split the line to tokens by white chars
 * then return a string array pointer
 */
char **vsh_split_line(const char *line) {
    char *new_line = vsh_prepare_line(line);
    
    char **args = (char **) malloc(VSH_ARGS_BUFF_SIZE);
    size_t arg_num = 0;
    
    char *token = strtok(new_line, VSH_BLANK_CHARS);;
    while (token != NULL) {
        args[arg_num] = (char *) malloc(VSH_ARG_BUFF_SIZE);
        strncpy(args[arg_num], token, VSH_ARG_BUFF_SIZE);
        
        arg_num++;
        token = strtok(NULL, VSH_BLANK_CHARS);
    }
    
    vsh_free(new_line);
    
    args[arg_num] = NULL;
    return args;
}

/*
 * check if a cmd is a builtin cmd
 */
bool vsh_is_builtin(char **args) {
    for (size_t idx = 0; idx < VSH_BUILTIN_NUM; idx++) {
        if (args[0] && strcmp(args[0], VSH_BUILTINS[idx]) == 0)
            return true;
    }
    return false;
}

/*
 * find special tokens (like "|") then split the tokens to a set of args
 * return an args array pointer
 */
char ***vsh_parse_args(char **args) {
    char ***argss = (char ***) malloc(VSH_ARG_BUFF_SIZE);
    size_t args_num = 0;
    size_t start_id = 0;
    for (size_t i = 0;; i++) {
        if (args[i] == NULL || strcmp(args[i], "|") == 0) {
            // save [start_id, i-1] -> argss[args_num]
            argss[args_num] = (char **) malloc((i - start_id + 1) * sizeof(char *));
            size_t arg_idx = 0;
            for (size_t k = start_id; k < i; k++) {
                argss[args_num][arg_idx++] = args[k];
            }
            argss[args_num][arg_idx] = NULL;
            args_num++;
            start_id = i + 1;
        }
        if (args[i] == NULL) {
            break;
        }
    }
    argss[args_num] = NULL;
    
    return argss;
}

/*
 * do pipeline recursively with an args array.
 * cur is the current cmd index, initial value should be 0.
 * in_fd will replace the cmd's stdin, initial value should be STDIN_FILENO.
 */
int vsh_pipeline(char ***argss, int cur, int in_fd) {
    char **args = argss[cur];
    if (argss[cur + 1] == NULL) {
        // last command
        if (in_fd != STDIN_FILENO) {
            int fd = dup2(in_fd, STDIN_FILENO);
            if (fd != -1) {
                close(in_fd);
            } else {
                vsh_error_exit("pipeline dup2");
            }
        }
        vsh_execute(args);
        vsh_error_exit("pipeline execvp");
        return 0;
    } else {
        // middle command
        int p[2];
        pid_t pid;
        
        if (-1 == pipe(p))
            vsh_error_exit("pipeline pipe");
        
        pid = fork();
        
        if (-1 == pid)
            vsh_error_exit("pipeline fork");
        
        if (pid == 0) {
            vsh_is_child = true;
            close(p[0]);
            if (-1 == dup2(in_fd, STDIN_FILENO))
                perror("in redirect failed");
            if (-1 == dup2(p[1], STDOUT_FILENO))
                perror("out redirect failed");
            else if (-1 == close(p[1]))
                perror("close dup fd failed");
            else {
                vsh_execute(args);
            }
        }
        
        close(p[1]);
        close(in_fd);
        
        int status;
        waitpid(pid, &status, 0);
        
        return vsh_pipeline(argss, cur + 1, p[0]);
    }
}

/*
 * shift array from pos to the first NULL.
 * and write NULL to the end pos.
 * a b c NULL
 *   ^ pos
 *
 */
void vsh_shift_util_null(char **args, size_t pos) {
    if (args[pos] == NULL) return;
    size_t idx;
    for (idx = pos; args[idx + 1]; idx++) {
        args[idx] = args[idx + 1];
    }
    args[idx] = NULL;
}

/*
 * find the redirect symbol in args.
 * if any one found, return the pos.
 * otherwise, return -1.
 */
int vsh_has_redirect(char **args) {
    for (size_t idx = 0; args[idx]; idx += 1) {
        for (size_t k = 0; k < VSH_REDIRECT_SYMBOL_NUM; k++) {
            if (strcmp(VSH_REDIRECT_SYMBOL[k], args[idx]) == 0) {
                return (int) idx;
            }
        }
    }
    return -1;
}

/*
 * DO NOT fork (cuz it's done by pipeline) and execute a command with args
 */
int vsh_execute(char **args) {
    if (!args || !args[0]) return 0;
    
    // check redirect
    int red_pos = vsh_has_redirect(args);
    if (red_pos != -1) {
        int red_fd;
        bool append = false;
        if (strcmp(args[red_pos], args[red_pos + 1]) == 0) {
            if (strcmp(args[red_pos], ">") == 0) {
                append = true;
                red_fd = STDOUT_FILENO;
            } else {
                // here doc
                perror("not impl");
                _exit(-1);
            }
        } else {
            if (strcmp(args[red_pos], ">") == 0) {
                // redirect stdout
                red_fd = STDOUT_FILENO;
            } else {
                // redirect stdin
                red_fd = STDIN_FILENO;
            }
        }
        
        int old_f = dup(red_fd);
        close(red_fd);
        int fd;
        if (red_fd == STDOUT_FILENO) {
            fd = open(args[append ? red_pos + 2 : red_pos + 1],
                          O_CREAT | O_WRONLY | (append ? O_APPEND : O_TRUNC),
                          S_IRUSR | S_IWUSR | S_IRGRP);
        } else {
            fd = open(args[red_pos + 1], O_RDONLY);
        }
        if (fd == -1) {
            perror("open file");
        }
        dup2(fd, red_fd);
        
        vsh_shift_util_null(args, red_pos);
        vsh_shift_util_null(args, red_pos);
        if (append) vsh_shift_util_null(args, red_pos);
        
        int ret = vsh_execute(args);
        
        close(fd);
        dup2(old_f, red_fd);
        return ret;
    }
    
    if (vsh_is_builtin(args)) {
        return vsh_exec_builtin(args);
    } else {
        execvp(args[0], args);
        perror(args[0]);
        _exit(-1);
    }
}

/*
 * execute builtin commands
 */
int vsh_exec_builtin(char **args) {
    if (strcmp(args[0], "cd") == 0) {
        // cd
        if (args[1])
            chdir(args[1]);
    } else if (strcmp(args[0], "pwd") == 0) {
        // pwd
        char wd[4096];
        puts(getcwd(wd, 4096));
    } else if (strcmp(args[0], "exit") == 0) {
        // exit
        exit(0);
    } else if (strcmp(args[0], "export") == 0) {
        char *cmd = strdup(args[1]);
        
        char *eq = strchr(cmd, '=');
        if (eq == NULL) return 0;
        *eq = '\0';
        setenv(cmd, eq + 1, true);
        
        vsh_free(cmd);
    }
    return 0;
}

/*
 * given an args, do the things that should be done.
 */
int vsh_run(char **args) {
    if (args[0] == NULL)
        return 0;
    
    int ret;
    
    char ***argss = vsh_parse_args(args);
    if (argss[1] == NULL && vsh_is_builtin(argss[0])) {
        // builtin (may change state of the shell)
        ret = vsh_execute(argss[0]);
    } else {
        // pipeline start !!
        pid_t pid = fork();
        if (pid == 0) {
            vsh_pipeline(argss, 0, STDIN_FILENO);
            _exit(-1);
        }
        
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            ret = WEXITSTATUS(status);
        } else {
            ret = -1;
        }
    }
    
    vsh_free(argss);
    return ret;
}

/*
 * main loop for the shell:
 *  - read
 *  - parse
 *  - execute
 */
void vsh_loop() {
    char *line;
    char **args;
    int ret;
    
    while (true) {
        vsh_prompt();
        line = vsh_get_line();
        args = vsh_split_line(line);
        
        last_exit_status = vsh_run(args);
        
        vsh_free(line);
        vsh_free(args);
    };
}

/*
 * called when main loop ends
 * do some clean work
 */
void vsh_clean() {}

/*
 * called before main loop runs
 * load shell configurations
 */
void vsh_init() {}