
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void pingpong() {
    /*
    int parent_fd[2], child_fd[2];
    pipe(parent_fd);
    pipe(child_fd);
    char buf[64];

    if (fork()) {
        // Parent
        write(parent_fd[1], "ping", strlen("ping"));
        read(child_fd[0], buf, 4);
        printf("%d: received %s\n", getpid(), buf);
    } else {
        // Child
        read(parent_fd[0], buf, 4);
        printf("%d: received %s\n", getpid(), buf);
        write(child_fd[1], "pong", strlen("pong"));
    }

    exit(0);
    */
    
    // printf("aaa");
    int parent_fd[2];
    int child_fd[2];
    

   pipe(parent_fd);
   pipe(child_fd);

    char p[10];
    int pid = fork();

    if (pid == 0) {
        // child
        read(parent_fd[0], p, 4);
        printf("%d: received %s\n", getpid(), p);
        write(child_fd[1], "pong", 4);
        // exit(0);
        
    } else {
        // parent

        write(parent_fd[1], "ping", 4);
        read(child_fd[0], p, 4);
        printf("%d: received %s\n", getpid(), p);
        // \n 
    }
    exit(0);
    
}