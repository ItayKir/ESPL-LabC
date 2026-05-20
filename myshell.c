#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "LineParser.h"
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

void execute(cmdLine* pCmdLine){
    pid_t pid = fork();
    if (pid == -1) {
        // fork failed!
        perror("fork failed");
        return; 
    }
    else if(pid==0){
        if(pCmdLine->inputRedirect != NULL){
            close(0);
            if (open(pCmdLine->inputRedirect, O_RDONLY) == -1) {    
                perror("Error opening input file");
                _exit(1);
            }
        }
        if(pCmdLine->outputRedirect != NULL){
            close(1);
            if (open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT , 0644) == -1) {
                perror("Error opening output file");
                _exit(1);
            }
        }
        execvp(pCmdLine->arguments[0], pCmdLine->arguments);
        fprintf(stderr, "Error executing command\n");
        _exit(1);
    }
    else{
        fprintf(stderr, "PID is: %d\n", pid);
        fprintf(stderr, "Executing program file name is: %s\n", pCmdLine -> arguments[0]);
        if(pCmdLine -> blocking){
            fprintf(stderr,"Running in Foreground.\n");
        }
        else{
            fprintf(stderr,"Running in Backgound.\n"); 
        }
        if(pCmdLine ->blocking){
            waitpid(pid, NULL, 0);
        }
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
        else{
            execute(pCmdLine);
        }
        freeCmdLines(pCmdLine);
    }
    return 0;
}