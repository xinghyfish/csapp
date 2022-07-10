#include "csapp.h"
int counter = 1;

int main(int argc, char const *argv[])
{
    if (fork() == 0) {
        counter--;
        exit(0);
    }
    else {
        Wait(NULL);
        printf("counter = %d\n", ++counter);
    }
    exit(0);
}
