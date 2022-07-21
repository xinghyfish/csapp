#include "csapp.h"

void end(void)
{
    printf("2");
    fflush(stdout);
}

int main(int argc, char const *argv[])
{
    if (Fork() == 0)
        atexit(end);
    if (Fork() == 0) {
        printf("0"); fflush(stdout);
    } else {
        printf("1"); fflush(stdout);
    }
    exit(0);
}
