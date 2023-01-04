#include "csapp.h"

int main(int argc, char const *argv[])
{
    int i;

    for (i = 0; i < 2; ++i)
        Fork();
    printf("hello\n");
    exit(0);
}
