#include "csapp.h"

int main(int argc, char const *argv[])
{
    int x = 3;

    if (Fork() != 0)
        printf("x=%d\n", ++x);

    printf("x=%d\n", --x);
    return 0;
}
