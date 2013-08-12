/*
 * Aaron Hayes 42338468
 * COMP3301 2013 - Assignment 1
 * qshell
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define BUFFER_SIZE 4096
#define CHAR_SIZE 1024
#define MAX_ARGS 25
#define MAX_LENGTH 128

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define SPACE " "
#define NEWLINE "\n"
#define PIPE "|"
#define REOUT ">"
#define REIN "<"
#define BACKGROUND "&"

#define READ 0
#define WRITE 1

typedef struct node {
    pid_t pid;
    int fd[2];
    struct node *next;
    int active;
    int num;
} Node;

typedef struct list {
    Node *head;
    Node *tail;
    int count;
} List;

typedef struct cmd {
    char *args[MAX_ARGS];
    int redirectIn;
    int redirectOut;
    int argc;
    char *inFile;
    char *outFile;
} Command;

typedef struct set{
    Command *cmd[2];
    int piping;
    int background;
    int check;
    int argc;
} Set;


/* Global Variables */
char *path;
struct sigaction sa;
pid_t pid[2];
pid_t masterpid;
List *background_processes;
int prompting = 1;

/* 
 * Destroy a linked list
 */
void destroy_list(List *l) {
    Node* c = l->head;
    while (c) {
        if (c->active) {
            close(c->fd[WRITE]);
            close(c->fd[READ]);
            kill(c->pid, SIGKILL);
            waitpid(c->pid, NULL, 0);
            c->active = 0;
        }
        Node* temp = c;
        c = c->next;
        free(temp);
    }
    free(l);
}

/*
 * Create a list of background processes
 */
List* create_list(void) {
    List *l = malloc(sizeof(List));
    l->head = l->tail = 0;
    l->count = 0;
    return l;
}

/*
 * Create a node for a background process
 */
void track_process(List * l, pid_t pid, int read, int write) {
    Node* new = malloc(sizeof(Node));
    new->next = 0;
    new->active = 1;
    new->pid = pid;
    new->fd[READ] = read;
    new->fd[WRITE] = write;
    new->num = l->count;
    
    if (l->head == 0) {    
        /* list was empty */
        l->head = l->tail = new;
    } else {
        /* n will be the new tail */
        l->tail->next = new;
        l->tail = new;
    }
}

/*
 * Check for completed background tasks
 */
void check_background_tasks(List *l) {
    int status;
    char *read = malloc(CHAR_SIZE * sizeof(char));
    Node *n = l->head;
    
    while (n != 0) {
        if(n->active) {
            /* Process was still running at last check, 
                check if it has finished */
            if(waitpid(n->pid, &status, WNOHANG) > 0) {
                n->active = 0;
                close(n->fd[WRITE]);
                FILE *in = fdopen(n->fd[READ], "r");
                while (fgets(read, CHAR_SIZE, in) != NULL) {
                    fprintf(stdout, "%s", read);
                }
                fflush(stdout);
                close(n->fd[READ]);
                fclose(in);
                fprintf(stdout, "[%d] (%d) Exited with status = %d\n", 
                    n->num, n->pid, WEXITSTATUS(status));
            }
        }
        n = n->next;
    }
    free(read);
}

/*
 * Run atexit of qshell
 * Wait for background processes to finish.
 * Cleanup and return resource.
 * exit.
 *
 */
void cleanup(void) {
    /* Only free memory/process if process is the main qshell process */
    if (masterpid == getpid()) {
        if(path) {
            free(path);
        }
        
        /* kill/reap foreground processes */
        if (pid[0] > 0) {
            kill(pid[0], SIGKILL);
            waitpid(pid[0], NULL, 0);
        }
        if (pid[1] > 0) {
            kill(pid[1], SIGKILL);
            waitpid(pid[1], NULL, 0);
        }
        
        /* kill/cleanup background processes */
        if(background_processes) {
            destroy_list(background_processes);
        }
    }

}


/*
 * Print prompt for user to input command.
 */
void prompt(void) {
    fflush(stdout);
    
    check_background_tasks(background_processes);
    
    getcwd(path, BUFFER_SIZE);
    
    if (prompting) {
        fprintf(stdout, "%s? ", path);
    }
    
    fflush(stdout);
}

/*
 * Signal Handler
 */
void sig_interrupt(int s) {
    int x = 0;
    int needPrompt = 1;
    int status;
    
    while (x < 2) {
        if (pid[x] > 0) {
            kill(pid[x], SIGINT);
            needPrompt = 0;
            waitpid(pid[x], &status, WNOHANG);
            if (WIFSIGNALED(status)) {
                pid[x] = 0;
            }
        }
        x++;
    }
    
    if (needPrompt) {
        fprintf(stdout, "\n");
        prompt();
    }
}

/*
 * Release and free memory allocated for a command
 */
void destroy_command(Command *c) {
    int n = 0;
    while (c->args[n]) {
        free(c->args[n]);
        n++;
    }

    if (c->outFile) free(c->outFile);
    if (c->inFile) free(c->inFile);
    
    free(c);
}

/*
 * Create a command
 */
Command* create_command(void) {
    Command *c = calloc(sizeof(Command), 1);
    c->argc = 0;
    c->redirectOut = 0;
    c->redirectIn = 0;
    c->inFile = 0;
    c->outFile = 0;
    memset(c->args, 0, MAX_ARGS);
    return c;
}

/*
 *  Create a set of commands
 */ 
Set* create_set(void) {
    Set *s =  calloc(sizeof(Set), 1);
    s->cmd[0] = create_command();
    s->cmd[1] = create_command();
    s->piping = 0;
    s->background = 0;
    s->check = 0;
    s->argc = 0;
    return s;
}

/*
 * Release and free memory allocated for a set 
 */
void destroy_set(Set *s) {
    if(s->cmd[0]) {
        destroy_command(s->cmd[0]);
    }
    if(s->cmd[1]) {
        destroy_command(s->cmd[1]);
    }
    free(s);
}

/*
 * Add argument to command
 */
void add_arg(Set *s, char* val, int c) {
    if (c >= 24) return;
    int p = s->piping;
    s->cmd[p]->args[c] = malloc(strlen(val) * sizeof(char) + 1);
    strncpy(s->cmd[p]->args[c], val, strlen(val) + 1);
} 

/*
 * Add a string to a set of commands
 */
void add(Set* s, char* val) {
    int p = s->piping;
    s->argc++;
    
    if (s->piping > 1) return;
    
    if(!strcmp(val, PIPE)) {
        s->piping++;
    } else if (!strcmp(val, BACKGROUND)) {
        s->background = 1;
    } else if (!strcmp(val, REOUT)) {
        s->cmd[p]->redirectOut = 2;
    } else if (!strcmp(val, REIN)) {
        s->cmd[p]->redirectIn = 2;
    } else if (s->cmd[p]->redirectIn == 2) {
        if(!s->cmd[p]->inFile) {
            s->cmd[p]->inFile = malloc(CHAR_SIZE * sizeof(char));
            strcpy(s->cmd[p]->inFile, val);
        } else {
            s->check = 1;
        }
        s->cmd[p]->redirectIn = 1;
    } else if (s->cmd[p]->redirectOut == 2) {
        if(!s->cmd[p]->outFile) {
            s->cmd[p]->outFile = malloc(CHAR_SIZE * sizeof(char));
            strcpy(s->cmd[p]->outFile, val);
        } else {
            s->check = 1;
        }
        s->cmd[p]->redirectOut = 1;
    } else {
        add_arg(s, val, s->cmd[p]->argc);
        s->cmd[p]->argc++;
    }
}

/*
 * Redirect a file stream 
 */
void redirect_filestream(int old, char *file, int flags, int mode, char*cmd) {
    int new;
    close(old);
    if((new = open(file, flags, mode)) == -1) {
        fprintf(stderr, "-qshell: %s: %s: Cannot open file\n", cmd, file);
        exit(EXIT_FAILURE);
    }
    dup2(new, old);
}


/*
 * Execute one command and pipe output into
 *  the input if another command
 */
void exec_pipe(Command *c1, Command *c2) {
    int fd[2];
    int status;

    if (pipe(fd)) {
        /* Pipe failed */
        fprintf(stderr, "An Error has occurred, exiting qshell...\n");
        exit(EXIT_FAILURE);
    }
    
    pid[0] = fork();
    if(!pid[0]) {
        /* Call Command one */
        close(STDOUT_FILENO);
        dup2(fd[WRITE], STDOUT_FILENO);
        if (c1->redirectIn) {
            redirect_filestream(STDIN_FILENO, c1->inFile, O_RDONLY, 
                0, c1->args[0]);
        }
        close(fd[READ]);
        execvp(c1->args[0], c1->args);
        close(fd[WRITE]);
        fprintf(stderr, "-qshell: %s: command not found\n", c1->args[0]);
        exit(EXIT_FAILURE);
    }
    pid[1] = fork();
    if(!pid[1]) {
        /* Call Command two */
        dup2(fd[READ], STDIN_FILENO);
        if (c2->redirectOut) {
            redirect_filestream(STDOUT_FILENO, c2->outFile, 
                O_WRONLY|O_CREAT|O_TRUNC, 
                S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, c2->args[0]);
        }
        close(fd[WRITE]);
        execvp(c2->args[0], c2->args);
        close(fd[READ]);
        fprintf(stderr, "-qshell: %s: command not found\n", c2->args[0]);
        exit(EXIT_FAILURE);
    }
    
    close(fd[READ]);
    close(fd[WRITE]);
    
    if ((waitpid(pid[0], &status, 0)) == -1) {
        kill(pid[0], SIGKILL);
        fprintf(stderr, "An Error has occurred, exiting qshell...\n");
        exit(EXIT_FAILURE);
    }
    pid[0] = 0;

    if ((waitpid(pid[1], &status, 0)) == -1) {
        kill(pid[1], SIGKILL);
        fprintf(stderr, "An Error has occurred, exiting qshell...\n");
        exit(EXIT_FAILURE);
    }
    pid[1] = 0;
}

/*
 * Execute given command in foreground
 */
void exec(Command *c) {
    int fd[2];
    int exitStat = 0;
    
    if (pipe(fd)) {
        /* Pipe failed */
        fprintf(stderr, "An Error has occurred, exiting qshell...\n");
        exit(EXIT_FAILURE);
    }
    
    pid[0] = fork();
    switch (pid[0]) {
        case -1:
            fprintf(stderr, "An Error has occurred, exiting qshell...\n");
            exit(EXIT_FAILURE);
            break;
        case 0:
            /* Child Process - Redirect Stdin/Stdout if required */
            if (c->redirectIn) {
                redirect_filestream(STDIN_FILENO, c->inFile, O_RDONLY, 
                    0, c->args[0]);
            }
            
            if (c->redirectOut) {
                redirect_filestream(STDOUT_FILENO, c->outFile, 
                    O_CREAT|O_WRONLY|O_TRUNC,
                    S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, 
                    c->args[0]);
            }
            
            execvp(c->args[0], c->args);
            /* Exec Failed At This Point */
            fprintf(stderr, "-qshell: %s: command not found\n", c->args[0]);
            exit(EXIT_FAILURE);
        default:
            /* Parent Process - Check if an error has occured */
            close(fd[READ]);
            if ((waitpid(pid[0], &exitStat, 0)) == -1) {
                kill(pid[0], SIGKILL);
                fprintf(stderr, "An Error has occurred, exiting qshell...\n");
                exit(EXIT_FAILURE);
            }
            pid[0]  = 0;
    }
}



/*
 * Execute given command in background
 */
void exec_background(Command *c) {
    int fd[2];
    pid_t pidbg;
    if (pipe(fd)) {
        /* Pipe failed */
        fprintf(stderr, "An Error has occurred, exiting qshell...\n");
        exit(EXIT_FAILURE);
    }
    
    pidbg = fork();
    
    switch (pidbg) {
        case -1:
            fprintf(stderr, "An Error has occurred, exiting qshell...\n");
            exit(EXIT_FAILURE);
            break;
        case 0:
            setsid();
            /* Child Process - Redirect Stdin/Stdout if required */
            if (c->redirectIn) {
                redirect_filestream(STDIN_FILENO, c->inFile, O_RDONLY, 
                    0, c->args[0]);
            } else {
                redirect_filestream(STDIN_FILENO, "/dev/null", O_RDONLY, 
                    0, c->args[0]);
                    close(fd[READ]);
            }
            
            if (c->redirectOut) {
                redirect_filestream(STDOUT_FILENO, c->outFile, 
                    O_CREAT|O_WRONLY|O_TRUNC,
                    S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,
                    c->args[0]);
            } else {
                close(STDOUT_FILENO);
                dup2(fd[WRITE], STDOUT_FILENO);
            }
            
            execvp(c->args[0], c->args);
            /* Exec Failed At This Point */
            fprintf(stderr, "-qshell: %s: command not found\n", c->args[0]);
            exit(EXIT_FAILURE);
        default:
            /* Parent Process - Check if an error has occurred */
            close(fd[WRITE]);
            fprintf(stdout, "[%d] %d\n", ++background_processes->count, pidbg);
            track_process(background_processes, pidbg, fd[READ], fd[WRITE]);
    }
}

/*
 * Correctly run command type
 */
void run_commands(Set* s) {
    if (s->background) {
        exec_background(s->cmd[0]);
    } else {
        if (s->piping) {
            exec_pipe(s->cmd[0], s->cmd[1]);
        } else {
            exec(s->cmd[0]);
        }
    }
} 

/*
 * Built in command for exit.
 * Exit qshell
 */
void ex(void) {
    exit(EXIT_SUCCESS);
}

/*
 * Built in comand for cd
 * Change user's working dir
 */
void cd(char *dir) {
    if (dir && strcmp(dir, "~")) {
        if(chdir(dir)) {
            fprintf(stderr, "-qshell: cd: %s: No such directory\n", dir);
        }
    } else {
        chdir(getenv("HOME"));
    }
}

/*
 * Checks values/arguments of a set are valid
 * Return 0 on valid set, 1 otherwise.
 */
int check_set(Set *s) {
    if (s->check) return 1;
    if (s->piping > 1) return 1;
    if (s->argc > 20) {
        return 1;
    }
    return 0;
}

/*
 * Read and phase input from user to generate correct commands.
 *  Send correct arguments to correct functions.
 */
void parseline(char *line)  {        
    if (strlen(line) > MAX_LENGTH) {
            fprintf(stderr, "-qshell: Command too long.\n");
            return;
    }
    strtok(line, NEWLINE);
    char *token = strtok(line, SPACE); 
    
    /* Call correct functions based on input */
    if (token != NULL && token[0] != '#' && token[0] != '\n') {

        /* Check for native commands */
        if (!strcmp(token, "exit")) {
            ex();
        } else if (!strcmp(token, "cd")) {
            int c = 0;
            char *dir = 0;
            while (token != NULL && c++ < 2) {
                if (c != 1) dir = token;
                token = strtok(NULL, SPACE);
            }
            cd(dir);
        } else {
            Set *s = create_set();
            while (token != NULL) {
                if(s->background) {
                    s->background = 0;
                }
                add(s, token);
                token = strtok(NULL, SPACE);
            }
            if(!check_set(s)) {
                run_commands(s);
            } else {
                fprintf(stderr, "-qshell: Command not supported.\n");
            }
            destroy_set(s);
        }
    }
}

/*
 * shell provides a shell for users to input commands.
 */
void shell(void) {
    char input[CHAR_SIZE];
    while (1) {
        prompt();
        if (feof(stdin)) break;
        fgets(input, CHAR_SIZE, stdin);
        parseline(input);
    }
}

/*
 * Entry into qshell. Check arguments.
 * Setup signal handlers, etc.
 */
int main (int argc, char *argv[]) {
    sa.sa_handler = sig_interrupt;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, 0);
    /*signal(SIGINT, sig_interrupt);*/
    if (argc == 2) {
        /* Read from file instead of standard in, don't display prompt */
        redirect_filestream(STDIN_FILENO, argv[1], O_RDONLY, 0, "qshell Input");
        prompting = 0;
    } else if (argc >= 3) {
        fprintf(stderr, "USAGE: qshell [file]\n");
        exit(EXIT_FAILURE);
    }
    masterpid = getpid();
    atexit(cleanup);
    path = calloc(BUFFER_SIZE * sizeof(char), 1);
    background_processes = create_list();
    
    shell();
    exit(EXIT_SUCCESS); 
}