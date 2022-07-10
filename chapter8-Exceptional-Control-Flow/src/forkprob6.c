#include "csapp.h"

void doit()
{
    if (Fork() == 0) {
        Fork();
        printf("hello\n");
        return;
    }
    return;
}

int main(int argc, char const *argv[])
{
    doit();
    printf("hello\n");
    exit(0);
}
