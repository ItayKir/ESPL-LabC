#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

int main(int argc, char **argv) {
    int pipefd[2];
    pid_t child1, child2;

    // Declare array of strings for execvp [cite: 16]
    char *cmd1[] = {"ps", "-xl", NULL};
    char *cmd2[] = {"grep", "5", NULL};

    // 1. Create a pipe [cite: 21]
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>forking…)\n"); [cite: 39]
    // 2. Fork a first child process (child1) [cite: 22]
    child1 = fork();
    if (child1 == -1) {
        perror("fork child1 failed");
        exit(EXIT_FAILURE);
    }

    if (child1 == 0) {
        // 3. In the child1 process [cite: 23]
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe…)\n"); [cite: 46]
        close(STDOUT_FILENO);     // Close the standard output [cite: 24]
        dup(pipefd[1]);           // Duplicate the write-end of the pipe [cite: 25]
        close(pipefd[1]);         // Close the duplicated file descriptor [cite: 26]
        close(pipefd[0]);         // Close the read-end, child1 doesn't need it

        fprintf(stderr, "(child1>going to execute cmd: %s %s…)\n", cmd1[0], cmd1[1]); [cite: 47]
        execvp(cmd1[0], cmd1);    // Execute "ps -xl" [cite: 27]
        
        perror("execvp ps failed");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>created process with id: %d)\n", child1); [cite: 40]

    // 4. In the parent process: Close the write end of the pipe [cite: 28]
    fprintf(stderr, "(parent_process>closing the write end of the pipe…)\n"); [cite: 41]
    close(pipefd[1]);

    fprintf(stderr, "(parent_process>forking…)\n"); [cite: 39]
    // 5. Fork a second child process (child2) [cite: 29]
    child2 = fork();
    if (child2 == -1) {
        perror("fork child2 failed");
        exit(EXIT_FAILURE);
    }

    if (child2 == 0) {
        // 6. In the child2 process [cite: 30]
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe…)\n"); [cite: 49]
        close(STDIN_FILENO);      // Close the standard input [cite: 31]
        dup(pipefd[0]);           // Duplicate the read-end of the pipe [cite: 32]
        close(pipefd[0]);         // Close the duplicated file descriptor [cite: 33]

        fprintf(stderr, "(child2>going to execute cmd: %s %s…)\n", cmd2[0], cmd2[1]); [cite: 50]
        execvp(cmd2[0], cmd2);    // Execute "grep 5" [cite: 34]
        
        perror("execvp grep failed");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>created process with id: %d)\n", child2); [cite: 40]

    // 7. In the parent process: Close the read end of the pipe [cite: 35]
    fprintf(stderr, "(parent_process>closing the read end of the pipe…)\n"); [cite: 42]
    close(pipefd[0]);

    // 8. Wait for the child processes to terminate, in the same order of their execution [cite: 36]
    fprintf(stderr, "(parent_process>waiting for child processes to terminate…)\n"); [cite: 43]
    waitpid(child1, NULL, 0);
    waitpid(child2, NULL, 0);

    fprintf(stderr, "(parent_process>exiting…)\n"); [cite: 44]
    return 0;
}