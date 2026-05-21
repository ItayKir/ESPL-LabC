#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

int main(int argc, char **argv) {
    int pipefd[2];
    pid_t child1, child2;

    char *cmd1[] = {"ps", "-xl", NULL};
    char *cmd2[] = {"grep", "5", NULL};

    // 1
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>forking…)\n"); 
    // 2
    child1 = fork();
    if (child1 == -1) {
        perror("fork child1 failed");
        exit(EXIT_FAILURE);
    }

    if (child1 == 0) {
        // 3
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe…)\n"); 
        close(STDOUT_FILENO);     
        dup(pipefd[1]);           
        close(pipefd[1]);         
        close(pipefd[0]);         

        fprintf(stderr, "(child1>going to execute cmd: %s %s…)\n", cmd1[0], cmd1[1]); 
        execvp(cmd1[0], cmd1);   
        
        perror("execvp ps failed");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>created process with id: %d)\n", child1); 

    // 4
    fprintf(stderr, "(parent_process>closing the write end of the pipe…)\n"); 
    close(pipefd[1]);

    fprintf(stderr, "(parent_process>forking…)\n"); 
    // 5
    child2 = fork();
    if (child2 == -1) {
        perror("fork child2 failed");
        exit(EXIT_FAILURE);
    }

    if (child2 == 0) {
        // 6
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe…)\n"); 
        close(STDIN_FILENO);      
        dup(pipefd[0]);           
        close(pipefd[0]);         

        fprintf(stderr, "(child2>going to execute cmd: %s %s…)\n", cmd2[0], cmd2[1]);
        execvp(cmd2[0], cmd2);    
        
        perror("execvp grep failed");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>created process with id: %d)\n", child2); 

    // 7
    fprintf(stderr, "(parent_process>closing the read end of the pipe…)\n"); 
    close(pipefd[0]);

    // 8
    fprintf(stderr, "(parent_process>waiting for child processes to terminate…)\n");
    waitpid(child1, NULL, 0);
    waitpid(child2, NULL, 0);

    fprintf(stderr, "(parent_process>exiting…)\n"); 
    return 0;
}