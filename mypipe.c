#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>




int main(int argc, char** argv){
    if (argc < 2) {
        fprintf(stderr, "Missing message to print\n");
        exit(1);
    }

    int pipefd[2];

    if(pipe(pipefd)==-1){
        fprintf(stderr, "Failed to create pipe!\n");
        exit(1);
    }
    pid_t pid = fork();
    if(pid == -1){
        fprintf(stderr, "Failed to fork!\n");
    }
    else if(pid > 0){
        write(pipefd[1], argv[1], strlen(argv[1]) + 1);
        close(pipefd[0]);
        close(pipefd[1]);
        exit(0);
    }
    else{
        char buffer[1000];

        read(pipefd[0], buffer, sizeof(buffer));
        printf("Child received: %s\n", buffer);

        close(pipefd[0]);
        close(pipefd[1]);
        exit(0);
    }
    return 0;
}