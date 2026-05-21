#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "LineParser.h"
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

// --- PART 3a & 3b: PROCESS MANAGER CONSTANTS & STRUCTURES ---
#define TERMINATED  -1
#define RUNNING 1
#define SUSPENDED 0

typedef struct process {
    cmdLine* cmd;                  /* the parsed command line*/
    pid_t pid;                     /* the process id that is running the command*/
    int status;                    /* status of the process: RUNNING/SUSPENDED/TERMINATED */
    struct process *next;          /* next process in chain */
} process;

// --- PART 3b: PROCESS MANAGER FUNCTIONS ---

void addProcess(process** process_list, cmdLine* cmd, pid_t pid) {
    process* new_process = (process*)malloc(sizeof(process));
    new_process->cmd = cmd;
    new_process->pid = pid;
    new_process->status = RUNNING; 
    
    new_process->next = *process_list;
    *process_list = new_process;
}

void freeProcessList(process* process_list) {
    process* curr = process_list;
    while (curr != NULL) {
        process* next = curr->next;
        freeCmdLines(curr->cmd); // Frees the associated parsed command
        free(curr);
        curr = next;
    }
}

void updateProcessStatus(process* process_list, int pid, int status) {
    process* curr = process_list;
    while (curr != NULL) {
        if (curr->pid == pid) {
            curr->status = status;
            return;
        }
        curr = curr->next;
    }
}

void updateProcessList(process **process_list) {
    process* curr = *process_list;
    
    while (curr != NULL) {
        int status;
        // WNOHANG: returns immediately if no child has exited.
        // WUNTRACED | WCONTINUED: reports if children stopped or resumed.
        pid_t ret = waitpid(curr->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
        
        if (ret == -1) {
            // Process doesn't exist anymore or was already reaped by foreground waitpid
            curr->status = TERMINATED;
        } else if (ret > 0) {
            // Status changed, evaluate what happened
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                curr->status = TERMINATED;
            } else if (WIFSTOPPED(status)) {
                curr->status = SUSPENDED;
            } else if (WIFCONTINUED(status)) {
                curr->status = RUNNING;
            }
        }
        curr = curr->next;
    }
}

void printProcessList(process** process_list) {
    updateProcessList(process_list); // 1. Refresh statuses
    printf("PID\t\tSTATUS\t\tCommand\n");
    
    process* curr = *process_list;
    process* prev = NULL;
    
    while (curr != NULL) {
        printf("%d\t\t", curr->pid);
        
        if (curr->status == RUNNING) {
            printf("Running\t\t");
        } else if (curr->status == SUSPENDED) {
            printf("Suspended\t");
        } else if (curr->status == TERMINATED) {
            printf("Terminated\t");
        }
        
        for (int i = 0; i < curr->cmd->argCount; i++) {
            printf("%s ", curr->cmd->arguments[i]);
        }
        printf("\n");
        
        // 2. Cleanup freshly terminated processes
        process* next = curr->next;
        if (curr->status == TERMINATED) {
            if (prev == NULL) {
                *process_list = next;
            } else {
                prev->next = next;
            }
            freeCmdLines(curr->cmd);
            free(curr);
        } else {
            prev = curr; // Only advance prev if we didn't delete the node
        }
        curr = next;
    }
}


// --- EXECUTION FUNCTIONS ---
void execute_single(cmdLine* pCmdLine, process** process_list){
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return; 
    }
    else if(pid==0){
        if(pCmdLine->inputRedirect != NULL){
            close(STDIN_FILENO);
            if (open(pCmdLine->inputRedirect, O_RDONLY) == -1) {    
                perror("Error opening input file");
                _exit(1);
            }
        }
        if(pCmdLine->outputRedirect != NULL){
            close(STDOUT_FILENO);
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
        addProcess(process_list, pCmdLine, pid);
        if(pCmdLine -> blocking){
            waitpid(pid, NULL, 0); // Reaps the child immediately for foreground tasks
        }
    }
}

void execute_pipeline(cmdLine* pCmdLine, process** process_list) {
    int pipefd[2];
    pid_t child1, child2;
    cmdLine* nextCmd = pCmdLine->next; 

    // Important Memory Fix: Sever the connection between LHS and RHS so 
    // freeCmdLines doesn't cause a double-free later when clearing the process list.
    pCmdLine->next = NULL; 

    if (pCmdLine->outputRedirect != NULL) {
        fprintf(stderr, "Error: Redirecting output of LHS process in a pipe is not allowed!\n");
        return;
    }
    if (nextCmd->inputRedirect != NULL) {
        fprintf(stderr, "Error: Redirecting input of RHS process in a pipe is not allowed!\n");
        return;
    }

    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return;
    }

    child1 = fork();
    if (child1 == -1) {
        perror("fork child1 failed");
        return;
    }
    if (child1 == 0) {
        if (pCmdLine->inputRedirect != NULL) {
            close(STDIN_FILENO);
            if (open(pCmdLine->inputRedirect, O_RDONLY) == -1) {
                perror("Error opening input file");
                _exit(EXIT_FAILURE);
            }
        }
        close(STDOUT_FILENO);
        dup(pipefd[1]);
        close(pipefd[1]);
        close(pipefd[0]);
        execvp(pCmdLine->arguments[0], pCmdLine->arguments);
        perror("Error executing LHS command");
        _exit(EXIT_FAILURE);
    }
    addProcess(process_list, pCmdLine, child1);

    close(pipefd[1]);

    child2 = fork();
    if (child2 == -1) {
        perror("fork child2 failed");
        return;
    }
    if (child2 == 0) {
        if (nextCmd->outputRedirect != NULL) {
            close(STDOUT_FILENO);
            if (open(nextCmd->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644) == -1) {
                perror("Error opening output file");
                _exit(EXIT_FAILURE);
            }
        }
        close(STDIN_FILENO);
        dup(pipefd[0]);
        close(pipefd[0]);
        execvp(nextCmd->arguments[0], nextCmd->arguments);
        perror("Error executing RHS command");
        _exit(EXIT_FAILURE);
    }
    addProcess(process_list, nextCmd, child2);
    close(pipefd[0]);

    if (nextCmd->blocking) {
        waitpid(child1, NULL, 0);
        waitpid(child2, NULL, 0);
    }
}

int main(int argc, char **argv)
{
    process* process_list = NULL; 
    int b = 1;
    
    while(b==1){
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        printf("Current working directory: %s\n", cwd);

        char input[2048];
        if (fgets(input, 2048, stdin) == NULL) break;
        
        cmdLine *pCmdLine = parseCmdLines(input);
        if(pCmdLine == NULL){
            continue;
        }
        
        int is_builtin = 1; 
        
        if(strcmp(pCmdLine -> arguments[0], "quit")==0){
            b=0;
        }
        else if(strcmp(pCmdLine -> arguments[0], "cd")==0){
            if(chdir(pCmdLine -> arguments[1])!=0){
                fprintf(stderr, "Failed to execute cd!\n");
            }
        }
        else if(strcmp(pCmdLine -> arguments[0], "procs")==0){
            printProcessList(&process_list);
        }
        else if(strcmp(pCmdLine -> arguments[0], "stop")==0){
            pid_t target_pid = atoi(pCmdLine -> arguments[1]);
            if(kill(target_pid, SIGSTOP)!=0){
                fprintf(stderr, "Failed to stop PID: %d!\n", target_pid);
            } else {
                printf("Process %d stopped.\n", target_pid);
                updateProcessStatus(process_list, target_pid, SUSPENDED);
            }
        }
        else if(strcmp(pCmdLine -> arguments[0], "wakeup")==0){
            pid_t target_pid = atoi(pCmdLine -> arguments[1]);
            if(kill(target_pid, SIGCONT)!=0){
                fprintf(stderr, "Failed to wakeup PID: %d!\n", target_pid);
            } else {
                printf("Process %d woken up.\n", target_pid);
                updateProcessStatus(process_list, target_pid, RUNNING);
            }
        }
        else if(strcmp(pCmdLine -> arguments[0], "ice")==0){
            pid_t target_pid = atoi(pCmdLine -> arguments[1]);
            if(kill(target_pid, SIGINT)!=0){
                fprintf(stderr, "Failed to ice PID: %d!\n", target_pid);
            } else {
                printf("Process %d iced.\n", target_pid);
                updateProcessStatus(process_list, target_pid, TERMINATED);
            }
        }
        else if(strcmp(pCmdLine -> arguments[0], "nuke")==0){
            pid_t target_pid = atoi(pCmdLine -> arguments[1]);
            if(kill(-target_pid, SIGKILL)!=0){
                fprintf(stderr, "Failed to nuke process group: %d!\n", target_pid);
            } else {
                printf("Process group %d nuked.\n", target_pid);
                // `updateProcessList` will catch these terminations automatically via waitpid next time procs is run
            }
        }
        else {
            is_builtin = 0; 
            if (pCmdLine->next != NULL) {
                execute_pipeline(pCmdLine, &process_list);
            } else {
                execute_single(pCmdLine, &process_list);
            }
        }
        
        if (is_builtin) {
            freeCmdLines(pCmdLine); 
        }
    }
    
    // Clean up process list before completely exiting
    freeProcessList(process_list);
    return 0;
}