#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "LineParser.h"
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

// Renamed from execute to execute_single for clarity
void execute_single(cmdLine* pCmdLine){
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return; 
    }
    else if(pid==0){
        // Handle input redirection
        if(pCmdLine->inputRedirect != NULL){
            close(STDIN_FILENO);
            if (open(pCmdLine->inputRedirect, O_RDONLY) == -1) {    
                perror("Error opening input file");
                _exit(1);
            }
        }
        // Handle output redirection
        if(pCmdLine->outputRedirect != NULL){
            close(STDOUT_FILENO);
            // Added O_TRUNC to ensure file is overwritten like standard bash behavior
            if (open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644) == -1) {
                perror("Error opening output file");
                _exit(1);
            }
        }
        execvp(pCmdLine->arguments[0], pCmdLine->arguments);
        perror("Error executing command");
        _exit(1);
    }
    else{
        fprintf(stderr, "PID is: %d\n", pid);
        fprintf(stderr, "Executing program file name is: %s\n", pCmdLine -> arguments[0]);
        if(pCmdLine -> blocking){
            fprintf(stderr,"Running in Foreground.\n");
            waitpid(pid, NULL, 0);
        }
        else{
            fprintf(stderr,"Running in Background.\n"); 
        }
    }
}

// New function to handle pipelines (e.g., ls | grep .c)
void execute_pipeline(cmdLine* pCmdLine) {
    int pipefd[2];
    pid_t child1, child2;
    cmdLine* nextCmd = pCmdLine->next; // The RHS command

    // 1. Validation: Prevent ambiguous/hanging redirections
    if (pCmdLine->outputRedirect != NULL) {
        fprintf(stderr, "Error: Redirecting the output of the LHS process in a pipe is not allowed!\n");
        return;
    }
    if (nextCmd->inputRedirect != NULL) {
        fprintf(stderr, "Error: Redirecting the input of the RHS process in a pipe is not allowed!\n");
        return;
    }

    // 2. Create the pipe
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return;
    }

    // 3. Fork Child 1 (LHS process)
    child1 = fork();
    if (child1 == -1) {
        perror("fork child1 failed");
        return;
    }
    
    if (child1 == 0) {
        // LHS Input Redirection (e.g. cat < in.txt | ...)
        if (pCmdLine->inputRedirect != NULL) {
            close(STDIN_FILENO);
            if (open(pCmdLine->inputRedirect, O_RDONLY) == -1) {
                perror("Error opening input file");
                _exit(EXIT_FAILURE);
            }
        }

        // Redirect stdout to the pipe's write-end
        close(STDOUT_FILENO);
        dup(pipefd[1]);
        close(pipefd[1]); // Close original write-end
        close(pipefd[0]); // Close unused read-end

        execvp(pCmdLine->arguments[0], pCmdLine->arguments);
        perror("Error executing LHS command");
        _exit(EXIT_FAILURE);
    }

    // 4. Parent Process: MUST close the write-end before forking child 2
    close(pipefd[1]);

    // 5. Fork Child 2 (RHS process)
    child2 = fork();
    if (child2 == -1) {
        perror("fork child2 failed");
        return;
    }

    if (child2 == 0) {
        // RHS Output Redirection (e.g. ... | tail -n 2 > out.txt)
        if (nextCmd->outputRedirect != NULL) {
            close(STDOUT_FILENO);
            if (open(nextCmd->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644) == -1) {
                perror("Error opening output file");
                _exit(EXIT_FAILURE);
            }
        }

        // Redirect stdin to the pipe's read-end
        close(STDIN_FILENO);
        dup(pipefd[0]);
        close(pipefd[0]); // Close original read-end

        execvp(nextCmd->arguments[0], nextCmd->arguments);
        perror("Error executing RHS command");
        _exit(EXIT_FAILURE);
    }

    // 6. Parent Process: Close the read-end
    close(pipefd[0]);

    // 7. Wait for both children (Check blocking on the LAST command)
    if (nextCmd->blocking) {
        waitpid(child1, NULL, 0);
        waitpid(child2, NULL, 0);
    }
}

int main(int argc, char **argv)
{
    int b = 1;
    while(b==1){
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        printf("Current working directory: %s\n", cwd);

        char input[2048];
        fgets(input, 2048, stdin);
        cmdLine *pCmdLine = parseCmdLines(input);
        
        if(pCmdLine == NULL){
            continue;
        }
        
        // Built-in commands
        if(strcmp(pCmdLine -> arguments[0], "quit")==0){
            b=0;
        }
        else if(strcmp(pCmdLine -> arguments[0], "cd")==0){
            if(chdir(pCmdLine -> arguments[1])!=0){
                fprintf(stderr, "Failed to execute cd!\n");
            }
        }
        else if(strcmp(pCmdLine -> arguments[0], "stop")==0){
            pid_t target_pid = atoi(pCmdLine -> arguments[1]);
            if(kill(target_pid, SIGSTOP)!=0){
                fprintf(stderr, "Failed to stop PID: %d!\n", target_pid);
            }
        }
        else if(strcmp(pCmdLine -> arguments[0], "wakeup")==0){
            pid_t target_pid = atoi(pCmdLine -> arguments[1]);
            if(kill(target_pid, SIGCONT)!=0){
                fprintf(stderr, "Failed to wakeup PID: %d!\n", target_pid);
            }
        }
        else if(strcmp(pCmdLine -> arguments[0], "ice")==0){
            pid_t target_pid = atoi(pCmdLine -> arguments[1]);
            if(kill(target_pid, SIGINT)!=0){
                fprintf(stderr, "Failed to ice PID: %d!\n", target_pid);
            }
        }
        else if(strcmp(pCmdLine -> arguments[0], "nuke")==0){
            pid_t target_pid = atoi(pCmdLine -> arguments[1]);
            if(kill(-target_pid, SIGKILL)!=0){
                fprintf(stderr, "Failed to nuke PID: %d!\n", target_pid);
            }
        }
        else {
            // Check if it's a pipeline or a single command
            if (pCmdLine->next != NULL) {
                execute_pipeline(pCmdLine);
            } else {
                execute_single(pCmdLine);
            }
        }
        freeCmdLines(pCmdLine); // Frees the entire linked list
    }
    return 0;
}