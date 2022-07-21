#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/* 创建4个子进程 */
void fork2()
{
    printf("L0\n");
    fork();
    printf("L1\n");
    fork();
    printf("Bye\n");
    exit(0);
}

/* 子进程直接打印Bye */
void fork4()
{
    printf("L0\n");
    if (fork() != 0) {
        printf("L1\n");
        if (fork() != 0) {
            printf("L2\n");
        }
    }
    printf("Bye\n");
    exit(0);
}

/* 父进程死循环，需要用kill杀死进程 */
void fork7()
{
    if (fork() == 0) {
        /* Child */
        printf("Terminating Child, PID = %d\n", getpid());
        exit(0);
    } else {
        printf("Running Parent, PID = %d\n", getpid());
        while (1)
        {
            ; /* Infinite Loop */
        }
        
    }
}

/* 子进程死循环，需要用kill杀死进程 */
void fork8() {
    if (fork() == 0) {
        /* Child */
        printf("Running Child, PID = %d\n", getpid());
        while (1) {
            ; /* Infinite Loop */
        }
    } else {
        printf("Terminating Parent, PID = %d\n", getpid());
        exit(0);
    }
}

/* wait等待子进程运行结束 */
void fork9() {
    int child_status;

    if (fork() == 0) {
        printf("HC: hello from child\n");
        exit(0);
    } else {
        printf("HP: hello from parent\n");
        wait(&child_status);
        printf("CT: child has terminated\n");
    }
    printf("Bye\n");
}

/* 创建N个子进程并等待其结束 */
void fork10() {
    int N = 10;
    pid_t pid[N];
    int i, child_status;

    for (i = 0; i < N; ++i) {
        if ((pid[i] = fork()) == 0) {
            exit(100 + i);  /* Child */
        }
    }
    for (i = 0; i < N; ++i) {
        pid_t wpid = wait(&child_status);
        if (WIFEXITED(child_status))
            printf("Child %d terminated with exit  status %d\n",
                    wpid, WEXITSTATUS(child_status));
        else
            printf("Child %d terminate abnormally\n", wpid);
    }
}

void fork11() {
    int N = 10;
    pid_t pid[N];
    int i;
    int child_status;

    for (i = 0; i < N; ++i) {
        if ((pid[i] = fork()) == 0) {
            exit(100 + i);  /* Child */
        }
    }
    for (i = N - 1; i >= 0; i--) {
        pid_t wpid = waitpid(pid[i], &child_status, 0);
        if (WIFEXITED(child_status))
            printf("Child %d terminated with exit  status %d\n",
                    wpid, WEXITSTATUS(child_status));
        else
            printf("Child %d terminate abnormally\n", wpid);
    }
}

int main(int argc, char const *argv[])
{
    fork11();
    exit(0);
}
