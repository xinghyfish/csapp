#include "csapp.h"

volatile sig_atomic_t pid;

void sigchld_handler(int s)
{
    int olderrno = errno;
    pid = Waitpid(-1, NULL, 0);
    errno = olderrno;
}

void sigint_handler(int s)
{
    /* Do nothing */
}

int main(int argc, char const *argv[])
{
    sigset_t mask, prev;

    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGINT, sigint_handler);
    Sigemptyset(&mask);
    Sigaddset(&mask, SIGCHLD);

    while (1)
    {
        Sigprocmask(SIG_BLOCK, &prev, NULL);
        if (Fork() == 0)  /* Child */
            exit(0);
        
        /* Parent */
        pid = 0;
        Sigprocmask(SIG_SETMASK, &prev, NULL);  /* Unblock SIGCHLD */

        /* Wait for SIGCHLD to be received (wasteful) */
        while (!pid)
            Sigsuspend(&prev);
        
        /* Do some work after receiving SIGCHLD */
        printf(".");
    }
    
    exit(0);
}
