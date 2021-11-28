#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char *argv[]) {
    int begin_fd[2];
    pipe(begin_fd);
    int pid = fork();

    if (pid == 0) {
        close(begin_fd[1]);
        prime(begin_fd[0]);
    } else {
        for (int i = 2; i <= 36; ++i) {
            write(begin_fd[1], (void *)&i, 1);
        }
        close(begin_fd[1]);
        close(begin_fd[0]);
    }

    // wait for child
    wait(0);
    // printf("ccc");
    exit(0);
}

void prime(int left_fd) {

    int num = 0;
    if (read(left_fd, (void *)&num, 1) == 0) {
        // printf("aaa");
        exit(0);
    }
    if (num > 35 || num == 0) {
        exit(0);
    }

    // printf("bbbb");
    int right_fd[2];
    pipe(right_fd);

    int pid = fork();

    if (pid == 0) {
        // child
        // close(left_fd);
        close(right_fd[1]);
        prime(right_fd[0]);
        // close(right_fd[0]);
    } else {
        close(right_fd[0]);

        printf("prime %d\n", num);

        int left_num = 0;
        while (read(left_fd, &left_num, 1)) {
            if (left_num % num != 0) {
                write(right_fd[1], &left_num, 1);
            }
        }
        close(right_fd[1]);
        // close(left_fd);
        // close(left_fd);
    }

    // wait for all child 
    wait(0);
    exit(0);
}